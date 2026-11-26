from __future__ import annotations

import json
import os
import re
import subprocess
import threading
from collections import deque
from pathlib import Path
from typing import Any, Self

# ---------------------------------------------------------------------------
#  LspError — raised for client-side / transport errors.
# ---------------------------------------------------------------------------

# Module-level executable path — overridden by the test harness
# (via ``set_exe_path``) or by the ``SLANG_EXE_PATH`` environment
# variable at import time.

_exe_path: Path = Path("build/Debug/slang").absolute()  # default fallback.


class LspError(Exception):
    """Raised when the LSP server reports an error or unexpected
    behaviour."""


def set_exe_path(path: Path | str) -> None:
    """Override the executable path used by ``launch_server``."""
    global _exe_path
    _exe_path = Path(path)


def get_exe_path() -> Path:
    return _exe_path


class LspClient:
    def __init__(
        self,
        stdin_fd: int,
        stdout_fd: int,
        server_process: subprocess.Popen,
    ) -> None:
        self.stdin_fd: int = stdin_fd
        self.stdout_fd: int = stdout_fd
        self.process: subprocess.Popen = server_process
        self.next_request_id: int = 1
        self.response_queue: deque[dict[str, Any]] = deque()
        self.notification_queue: deque[dict[str, Any]] = deque()
        self._running: bool = True
        self._reader_thread: threading.Thread | None = None

        # Single condition variable — ``self`` as its internal lock.
        self._cv: threading.Condition = threading.Condition()

    # .......................................................................
    #  Factory — launch the ``lsp --server`` subprocess.
    # .......................................................................

    @staticmethod
    def launch_server(
        exe_path: Path | str | None = None,
    ) -> LspClient:
        """Launch *exe_path* as ``lsp --server`` and return a
        configured :class:`LspClient` whose stdin/stdout are
        connected to the child process.

        *``exe_path``* may be omitted (the method returns a
        "stub" with no active subprocess) — in that case it
        falls back to the module-level ``_exe_path`` variable,
        which the test harness sets via ``set_exe_path()``.

        Parameters
        ----------
        exe_path : str, optional
            Path to the ``slang`` executable (relative or absolute).

        Raises
        ------
        LspError
            If no path is provided and no fallback is set.
        """
        if not exe_path:
            exe_path = _exe_path
        else:
            exe_path = Path(exe_path)

        if not exe_path.exists():
            raise FileNotFoundError(exe_path)

        cmd = [str(exe_path), "lsp", "--server"]

        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        if proc.stdin is None or proc.stdout is None:
            proc.terminate()
            raise LspError("Could not open stdin/stdout for subprocess.")

        return LspClient(
            stdin_fd=proc.stdin.fileno(),
            stdout_fd=proc.stdout.fileno(),
            server_process=proc,
        )

    # .......................................................................
    #  Lifecycle: __enter__ / __exit__
    # .......................................................................

    def __enter__(self) -> Self:
        """Start the reader thread and send ``initialize``."""
        self._start_reader()
        req_id = self.next_request_id
        self.next_request_id += 1
        payload = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": req_id,
                "method": "initialize",
                "params": {},
            },
            separators=(",", ":"),
        ).encode("utf-8")
        self._send_frame_sync(payload)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Initiate orderly shutdown (``shutdown`` + ``exit``)."""
        with self._cv:
            self._shutdown_internal()
        self._stop_reader(force=False)
        try:
            self.process.stdin.close()  # type: ignore[union-attr]
        except Exception:  # noqa
            pass
        try:
            self.process.stdout.close()  # type: ignore[union-attr]
        except Exception:  # noqa
            pass
        try:
            self.process.stderr.close()  # type: ignore[union-attr]
        except Exception:  # noqa
            pass
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()

    # .......................................................................
    #  Init handshake
    # .......................................................................

    def wait_for_init(self) -> bool:
        """Block until the server's ``initialize`` response arrives,
        send the ``initialized`` notification, and return ``True``.

        If the server crashes or never responds, return ``False``.

        Call this **after** entering the context (``lsp.__enter__()``
        or ``with lsp:``) and before sending any other requests.
        """
        init_id: int = self.next_request_id - 1
        deadline = _monotonic() + 5.0

        while _monotonic() < deadline:
            with self._cv:
                while self.response_queue:
                    msg = self.response_queue.popleft()
                    if msg.get("id") == init_id:
                        self._send_init_notification()
                        return True
                    # Unrelated message — keep waiting.

                # Wait with small timeout so we exit promptly if
                # the server has died.
                self._cv.wait(timeout=0.02)

        return False

    # .......................................................................
    #  Background reader thread
    # .......................................................................

    def _reader_loop(self) -> None:
        """Continuously read framed messages from stdout.

        Responses (with an ``id``) go into the shared queue and the
        condition is notified.  Notifications (without ``id``) are
        silently consumed.
        """
        read_buf = b""
        while self._running:
            chunk = None
            try:
                chunk = os.read(self.stdout_fd, 8192)
                if not chunk:
                    self._running = False
                    break
            except OSError:
                self._running = False
                break

            read_buf += chunk

            # --- frame boundaries ---
            while b"\r\n\r\n" in read_buf:
                header_end = read_buf.index(b"\r\n\r\n")
                headers = read_buf[:header_end]
                body_start = header_end + 4

                # Extract Content-Length.
                try:
                    header_text = headers.decode("ascii")
                except UnicodeDecodeError:
                    self._running = False
                    break

                match = re.search(
                    r"Content-Length:\s*(\d+)",
                    header_text,
                )
                if not match:
                    break

                content_length = int(match.group(1))
                available = len(read_buf) - body_start
                if available < content_length:
                    break  # not enough data yet.

                body_bytes = read_buf[body_start : body_start + content_length]
                read_buf = read_buf[body_start + content_length :]

                # Parse JSON.
                try:
                    message = json.loads(body_bytes.decode("utf-8"))
                except (json.JSONDecodeError, UnicodeDecodeError):
                    break

                # Enqueue response (has id) or notification (no id).
                with self._cv:
                    if "id" in message:
                        self.response_queue.append(message)
                    else:
                        self.notification_queue.append(message)
                    self._cv.notify_all()

    def _start_reader(self) -> None:
        """Start the background reader thread."""
        self._running = True
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()

    def _stop_reader(self, force: bool = False) -> None:
        self._running = False
        if self._reader_thread and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=0.5 if not force else 0)

    # .......................................................................
    #  Requests (synchronous)
    # .......................................................................

    def send_request(self, method: str, params: dict | None = None) -> dict[str, Any]:
        """Send a JSON-RPC *request* and block until the server
        responds.

        The main thread writes the frame to stdin, then blocks on
        ``self._cv.wait()`` — the reader thread calls
        ``notify_all()`` as soon as a response arrives.

        Parameters
        ----------
        method : str
            LSP method name (e.g. ``"textDocument/hover"``).
        params : dict, optional
            Optional parameters dict.

        Returns
        -------
        dict
            The server's response (may contain an ``error`` key).

        Raises
        ------
        LspError
            If no response arrives within 10 seconds.
        """
        req_id = self.next_request_id
        message: dict[str, Any] = {
            "jsonrpc": "2.0",
            "id": req_id,
            "method": method,
        }
        if params is not None:
            message["params"] = params

        self.next_request_id += 1
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")

        # Send the frame.  Lock-free — ``os.write`` is atomic for
        # small payloads (under PIPE_BUF, typically 4 KB).
        self._send_frame_sync(payload)

        deadline = _monotonic() + 10.0

        while _monotonic() < deadline:
            with self._cv:
                for idx, resp in enumerate(self.response_queue):
                    if resp.get("id") == req_id:
                        del self.response_queue[idx]
                        return resp
                self._cv.wait(timeout=0.02)

        raise LspError("Timed out waiting for server response.")

    # .......................................................................
    #  Notifications (fire-and-forget)
    # .......................................................................

    def send_notification(
        self,
        method: str,
        params: dict | None = None,
    ) -> None:
        """Send a JSON-RPC *notification* (fire-and-forget)."""
        message: dict[str, Any] = {
            "jsonrpc": "2.0",
            "method": method,
        }
        if params is not None:
            message["params"] = params

        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")
        self._send_frame_sync(payload)

    def wait_for_diagnostics(self, timeout: float = 10.0) -> dict[str, Any] | None:
        """Block until a
        ``textDocument/publishDiagnostics`` notification arrives (or
        *timeout* seconds elapse).  Returns the ``params`` dict, or
        ``None``.
        """
        deadline = _monotonic() + timeout
        while _monotonic() < deadline:
            with self._cv:
                while self.notification_queue:
                    msg = self.notification_queue.popleft()
                    if msg.get("method") == "textDocument/publishDiagnostics":
                        return msg.get("params")
                self._cv.wait(timeout=0.02)
        return None

    # .......................................................................
    #  Frame I/O
    # .......................................................................

    def _send_frame_sync(self, payload: bytes) -> None:
        """Write a framed message to stdin.  No lock needed —
        ``os.write`` is atomic for small payloads."""
        header = (f"Content-Length: {len(payload)}\r\n\r\n").encode()
        written = os.write(self.stdin_fd, header + payload)
        if written != len(header) + len(payload):
            raise LspError(
                f"Partial write: expected {len(header) + len(payload)}, got {written}"
            )

    # .......................................................................
    #  Init / Shutdown internals (caller must hold _cv)
    # .......................................................................

    def _shutdown_internal(self) -> None:
        """Send ``shutdown`` request + ``exit``.  Caller must hold
        ``self._cv``."""
        try:
            payload = json.dumps(
                {
                    "jsonrpc": "2.0",
                    "id": self.next_request_id,
                    "method": "shutdown",
                },
                separators=(",", ":"),
            ).encode("utf-8")
            self.next_request_id += 1
            self._send_frame_sync(payload)
        except (LspError, Exception):  # noqa
            pass

        try:
            payload = json.dumps(
                {"jsonrpc": "2.0", "method": "exit"},
                separators=(",", ":"),
            ).encode("utf-8")
            self._send_frame_sync(payload)
        except (LspError, Exception):  # noqa
            pass

    def _send_init_notification(self) -> None:
        """Send the ``initialized`` notification.  Caller must hold
        ``self._cv``."""
        try:
            payload = json.dumps(
                {"jsonrpc": "2.0", "method": "initialized"},
                separators=(",", ":"),
            ).encode("utf-8")
            self._send_frame_sync(payload)
        except (LspError, Exception):  # noqa
            pass


# === Module-level helpers ===


def _monotonic() -> float:
    """Return the current monotonic clock value in seconds."""
    import time

    return time.monotonic()
