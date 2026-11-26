"""End-to-end LSP completion tests against the slang executable."""

from __future__ import annotations

import os
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
        server.send_notification(
            "workspace/didChangeConfiguration",
            {
                "settings": {
                    "slang": {
                        "searchPaths": [str(_HERE.parents[1] / "lang")],
                    }
                }
            },
        )
        yield server


def _complete(lsp: client.LspClient, uri: str, line: int, character: int) -> list[dict]:
    response = lsp.send_request(
        "textDocument/completion",
        {
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character},
        },
    )
    assert response.get("jsonrpc") == "2.0"
    assert "error" not in response
    assert isinstance(response.get("result"), list)
    return response["result"]


def test_completion_struct_members_e2e(lsp: client.LspClient) -> None:
    uri = (_HERE / "fixtures" / "completion_struct.sl").as_uri()
    source = (
        "struct Vec2 {\n"
        "    x: f32,\n"
        "    y: f32\n"
        "};\n"
        "\n"
        "fn main() -> i32 {\n"
        "    let v: Vec2 = Vec2 { x: 1.0 as f32, y: 2.0 as f32 };\n"
        "    v.\n"
        "    return 0;\n"
        "}\n"
    )
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": source}},
    )
    lsp.wait_for_diagnostics(timeout=2.0)

    items = _complete(lsp, uri, line=7, character=6)
    labels = {item["label"]: item for item in items}
    assert set(labels.keys()) == {"x", "y"}
    assert labels["x"]["kind"] == 5  # CompletionItemKind.Field
    assert labels["x"]["detail"] == "f32"
    assert labels["y"]["kind"] == 5
    assert labels["y"]["detail"] == "f32"


def test_completion_imported_struct_members_e2e(lsp: client.LspClient) -> None:
    uri = (_HERE / "fixtures" / "completion_import.sl").as_uri()
    source = (
        "import std;\n"
        "\n"
        "fn inspect() -> i32 {\n"
        '    let res: std::result = std::parse_i32("42");\n'
        "    if(res.) {\n"
        "        return 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    )
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": source}},
    )
    lsp.wait_for_diagnostics(timeout=2.0)

    items = _complete(lsp, uri, line=4, character=11)
    labels = {item["label"]: item for item in items}
    assert "ok" in labels
    assert "value" in labels
    assert labels["ok"]["kind"] == 5
