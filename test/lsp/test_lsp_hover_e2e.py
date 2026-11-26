"""End-to-end LSP hover tests against the slang executable.

Launch the actual ``lsp --server`` executable, open a document via
``textDocument/didOpen``, send ``textDocument/hover`` requests and
validate the JSON-RPC responses.

Run with ``python -m pytest test/lsp``. All paths are derived from
``__file__`` so the process working directory is irrelevant.
"""

from __future__ import annotations

import os
import sys
from collections.abc import Iterator
from pathlib import Path

import pytest

# Resolve the test/lsp directory (where this file lives).
_HERE = Path(__file__).parent.resolve()

sys.path.insert(0, str(_HERE))  # ``import client`` works.

import client  # LspClient, LspError.

_exe = os.environ.get("SLANG_EXE_PATH")
if _exe:
    client.set_exe_path(_exe)


def _fixture_uri(name: str) -> str:
    return _fixture(name).as_uri()


def _fixture(name: str) -> Path:
    """Return the absolute path to *name* in the fixtures directory."""
    return _HERE / "fixtures" / name


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


def _open_fixture(lsp: client.LspClient, name: str) -> str:
    """Open a fixture document and wait for its diagnostics notification."""
    text = _fixture(name).read_text(encoding="utf-8")
    uri = _fixture_uri(name)
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": text}},
    )
    lsp.wait_for_diagnostics(timeout=2.0)
    return uri


def _hover(lsp: client.LspClient, uri: str, line: int, character: int) -> dict:
    response = lsp.send_request(
        "textDocument/hover",
        {
            "textDocument": {"uri": uri},
            "position": {"line": line, "character": character},
        },
    )
    assert response.get("jsonrpc") == "2.0"
    assert "error" not in response
    assert "result" in response
    return response["result"]


def test_hover_const(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_test.sl")
    result = _hover(lsp, uri, line=8, character=7)
    assert result is not None
    assert result.get("contents")


def test_hover_function(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_function.sl")
    result = _hover(lsp, uri, line=5, character=3)
    assert "fn myAdd(a: i32, b: i32) -> i32" in result["contents"]["value"]


def test_hover_variable(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_test.sl")
    result = _hover(lsp, uri, line=11, character=8)
    assert "str" in result["contents"]["value"]


def test_hover_type(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_test.sl")
    result = _hover(lsp, uri, line=8, character=16)
    assert "type i32" in result["contents"]["value"]


def test_hover_imported_function_and_type(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_import.sl")
    source = _fixture("hover_import.sl").read_text(encoding="utf-8")
    function_line = next(
        i for i, line in enumerate(source.splitlines()) if "std::println" in line
    )
    function_character = source.splitlines()[function_line].index("println")
    function_result = _hover(lsp, uri, function_line, function_character)
    assert "fn println(" in function_result["contents"]["value"]
    assert "str" in function_result["contents"]["value"]

    type_line = next(
        i for i, line in enumerate(source.splitlines()) if "std::i32s" in line
    )
    type_character = source.splitlines()[type_line].index("i32s")
    type_result = _hover(lsp, uri, type_line, type_character)
    assert "std::i32s" in type_result["contents"]["value"]


def test_hover_unknown_position(lsp: client.LspClient) -> None:
    uri = _open_fixture(lsp, "hover_test.sl")
    result = _hover(lsp, uri, line=9, character=0)
    assert result in (None, {})


def test_hover_struct_member_and_array_length(lsp: client.LspClient) -> None:
    uri = _fixture_uri("hover_struct_member.sl")
    source = (
        "struct Point {\n"
        "    x: i32,\n"
        "    y: f32\n"
        "};\n"
        "fn main(args: str[]) -> i32 {\n"
        "    let p: Point = Point { x: 1, y: 2.0 as f32 };\n"
        "    let _x: i32 = p.x;\n"
        "    return args.length;\n"
        "}\n"
    )
    lsp.send_notification(
        "textDocument/didOpen",
        {"textDocument": {"uri": uri, "version": 1, "text": source}},
    )
    lsp.wait_for_diagnostics(timeout=2.0)

    member_res = _hover(lsp, uri, line=6, character=20)
    assert "Point.x" in member_res["contents"]["value"]
    assert "i32" in member_res["contents"]["value"]

    len_res = _hover(lsp, uri, line=7, character=16)
    assert "str[].length" in len_res["contents"]["value"]
    assert "i32" in len_res["contents"]["value"]
