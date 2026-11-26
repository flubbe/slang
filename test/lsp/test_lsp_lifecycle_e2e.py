"""End-to-end LSP lifecycle, diagnostics, and navigation tests."""

from __future__ import annotations

import os
import subprocess
import sys
from collections.abc import Iterator
from pathlib import Path

import pytest

_HERE = Path(__file__).parent.resolve()
sys.path.insert(0, str(_HERE))

import client  # LspClient

_exe = os.environ.get("SLANG_EXE_PATH")
if _exe:
    client.set_exe_path(_exe)


@pytest.fixture
def lsp() -> Iterator[client.LspClient]:
    exe = client.get_exe_path()
    with client.LspClient.launch_server(Path(exe).absolute()) as server:
        assert server.wait_for_init(), f"Server failed to initialize using '{exe}'."
        yield server


def test_unknown_method_returns_method_not_found(lsp: client.LspClient) -> None:
    response = lsp.send_request("slang/nonexistentMethod", {})
    assert "error" in response
    assert response["error"]["code"] == -32601


def test_diagnostics_lifecycle_open_change_close(lsp: client.LspClient) -> None:
    uri = (_HERE / "fixtures" / "diag_lifecycle.sl").as_uri()

    # Open with syntax error.
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": "fn main() {"}},
    )
    diag1 = lsp.wait_for_diagnostics(timeout=2.0)
    assert diag1 is not None
    assert diag1["uri"] == uri
    assert len(diag1["diagnostics"]) > 0

    # Fix syntax error via didChange.
    lsp.send_notification(
        "textDocument/didChange",
        {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": "fn main() -> i32 { return 0; }"}],
        },
    )
    diag2 = lsp.wait_for_diagnostics(timeout=2.0)
    assert diag2 is not None
    assert diag2["uri"] == uri
    assert diag2["diagnostics"] == []

    # Close clears diagnostics.
    lsp.send_notification("textDocument/didClose", {"textDocument": {"uri": uri}})
    diag3 = lsp.wait_for_diagnostics(timeout=2.0)
    assert diag3 is not None
    assert diag3["uri"] == uri
    assert diag3["diagnostics"] == []


def test_navigation_symbols_definition_references_e2e(
    lsp: client.LspClient,
) -> None:
    uri = (_HERE / "fixtures" / "nav_e2e.sl").as_uri()
    source = (
        "struct Point { x: i32, y: i32 };\n"
        "fn helper(a: i32) -> i32 { return a; }\n"
        "fn main() -> i32 { return helper(1); }\n"
    )
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": source}},
    )
    lsp.wait_for_diagnostics(timeout=2.0)

    # Document symbols.
    sym_resp = lsp.send_request(
        "textDocument/documentSymbol", {"textDocument": {"uri": uri}}
    )
    assert "error" not in sym_resp
    names = {s["name"] for s in sym_resp["result"]}
    assert {"Point", "helper", "main"}.issubset(names)

    # Go to definition on 'helper' in 'main' (line 2, char 26).
    def_resp = lsp.send_request(
        "textDocument/definition",
        {
            "textDocument": {"uri": uri},
            "position": {"line": 2, "character": 26},
        },
    )
    assert "error" not in def_resp
    assert def_resp["result"] is not None
    assert def_resp["result"]["uri"] == uri
    assert def_resp["result"]["range"]["start"]["line"] == 1

    # Find references on 'helper'.
    ref_resp = lsp.send_request(
        "textDocument/references",
        {
            "textDocument": {"uri": uri},
            "position": {"line": 2, "character": 26},
            "context": {"includeDeclaration": True},
        },
    )
    assert "error" not in ref_resp
    assert len(ref_resp["result"]) >= 2


def test_shutdown_rejects_subsequent_request_and_exits_zero() -> None:
    exe = Path(client.get_exe_path()).absolute()
    with client.LspClient.launch_server(exe) as server:
        assert server.wait_for_init()
        shutdown_resp = server.send_request("shutdown")
        assert shutdown_resp.get("result") is None

        after_resp = server.send_request("textDocument/hover", {})
        assert "error" in after_resp
        assert after_resp["error"]["code"] == -32600

        server.send_notification("exit")
        rc = server.process.wait(timeout=5.0)
        assert rc == 0


def test_exit_without_shutdown_exits_nonzero() -> None:
    exe = Path(client.get_exe_path()).absolute()
    server = client.LspClient.launch_server(exe)
    with server:
        assert server.wait_for_init()
        server.send_notification("exit")
        rc = server.process.wait(timeout=5.0)
        assert rc != 0


@pytest.mark.skip("Not reliably implemented")
def test_compiled_module_definition_and_hover_from_source(
    tmp_path: Path,
) -> None:
    exe = Path(client.get_exe_path()).absolute()
    mod_sl = tmp_path / "mathlib.sl"
    mod_cmod = tmp_path / "mathlib.cmod"
    mod_sl.write_text(
        "struct Vec2 {\n"
        "    x: i32,\n"
        "    y: i32\n"
        "};\n"
        "const SCALE: i32 = 7;\n"
        "fn add(left: i32, right: i32) -> i32 {\n"
        "    return left + right;\n"
        "}\n",
        encoding="utf-8",
    )
    compile_res = subprocess.run(
        [str(exe), "compile", str(mod_sl), "-o", str(mod_cmod)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert compile_res.returncode == 0, compile_res.stderr

    main_sl = tmp_path / "main.sl"
    main_source = (
        "import mathlib;\n"
        "fn main() -> i32 {\n"
        "    let v: mathlib::Vec2 = mathlib::Vec2 { x: 1, y: 2 };\n"
        "    return mathlib::add(v.x, mathlib::SCALE);\n"
        "}\n"
    )
    main_uri = main_sl.as_uri()
    mod_uri = mod_sl.resolve().as_uri()

    with client.LspClient.launch_server(exe) as server:
        assert server.wait_for_init()
        server.send_notification(
            "workspace/didChangeConfiguration",
            {"settings": {"slang": {"searchPaths": [str(tmp_path)]}}},
        )
        server.send_notification(
            "textDocument/didOpen",
            {"textDocument": {"uri": main_uri, "version": 1, "text": main_source}},
        )
        server.wait_for_diagnostics(timeout=2.0)

        # Go to definition: mathlib::add (line 3, col 20).
        def_add = server.send_request(
            "textDocument/definition",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 3, "character": 20},
            },
        )
        assert def_add["result"]["uri"] == mod_uri
        assert def_add["result"]["range"]["start"]["line"] == 5

        # Go to definition: mathlib::SCALE (line 3, col 38).
        def_scale = server.send_request(
            "textDocument/definition",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 3, "character": 38},
            },
        )
        assert def_scale["result"]["uri"] == mod_uri
        assert def_scale["result"]["range"]["start"]["line"] == 4

        # Go to definition: mathlib::Vec2 (line 2, col 20).
        def_vec = server.send_request(
            "textDocument/definition",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 2, "character": 20},
            },
        )
        assert def_vec["result"]["uri"] == mod_uri
        assert def_vec["result"]["range"]["start"]["line"] == 0

        # Go to definition: v.x (line 3, col 26).
        def_field = server.send_request(
            "textDocument/definition",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 3, "character": 26},
            },
        )
        assert def_field["result"]["uri"] == mod_uri
        assert def_field["result"]["range"]["start"]["line"] == 1

        # Hover: mathlib::add shows real parameter names from mathlib.sl.
        hover_add = server.send_request(
            "textDocument/hover",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 3, "character": 20},
            },
        )
        assert (
            "fn add(left: i32, right: i32) -> i32"
            in hover_add["result"]["contents"]["value"]
        )

        # Hover: mathlib::SCALE shows constant value from mathlib.sl.
        hover_scale = server.send_request(
            "textDocument/hover",
            {
                "textDocument": {"uri": main_uri},
                "position": {"line": 3, "character": 38},
            },
        )
        assert "i32 = 7" in hover_scale["result"]["contents"]["value"]
