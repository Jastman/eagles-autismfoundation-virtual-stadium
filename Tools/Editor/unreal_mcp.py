"""Minimal Unreal MCP client (streamable HTTP, JSON-RPC) for scripts in Tools/Editor.

Talks to the editor's Unreal MCP server (Docs/LOCAL_AGENT_SETUP.md §2) at 127.0.0.1 only.
Toolset tools are dispatched through `call_tool` with their short name.
"""
import json
import os
import urllib.error
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
_HEADERS = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
_session_id = None


class McpError(RuntimeError):
    pass


def _post(payload, session_id=None):
    headers = dict(_HEADERS)
    if session_id:
        headers["Mcp-Session-Id"] = session_id
    request = urllib.request.Request(URL, data=json.dumps(payload).encode(), headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=300) as response:
        body = response.read().decode()
        return response.headers.get("Mcp-Session-Id"), (json.loads(body) if body.strip() else None)


def _session():
    global _session_id
    if _session_id is None:
        _session_id, _ = _post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-06-18", "capabilities": {},
            "clientInfo": {"name": os.path.basename(__file__), "version": "1"}}})
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, _session_id)
    return _session_id


def call(toolset, tool, arguments=None):
    """Calls a toolset tool and returns its decoded `returnValue`. Raises McpError on tool errors."""
    _, response = _post({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
        "name": "call_tool",
        "arguments": {"toolset_name": toolset, "tool_name": tool, "arguments": arguments or {}}}}, _session())
    if "error" in response:
        raise McpError(f"{tool}: {response['error']}")
    result = response["result"]
    text = "\n".join(c.get("text", "") for c in result.get("content", []))
    if result.get("isError"):
        raise McpError(f"{tool}: {text}")
    value = json.loads(text).get("returnValue") if text.strip().startswith("{") else text
    if isinstance(value, str) and value.strip().startswith(("{", "[")):
        value = json.loads(value)
    return value
