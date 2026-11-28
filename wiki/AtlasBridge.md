# Connecting an agent to the editor

Atlas Bridge lets an agent (Claude Code, Codex, ChatGPT, ...) change and look at the scene that is open in the editor
while you watch, and run jobs with the real GPU on scene files. Every program linking the bridge library can offer a
[Model Context Protocol](https://modelcontextprotocol.io) server on `http://127.0.0.1:7777/mcp`: the editor with a
project open, `AtlasBridge.exe mcp`, later games. The first program that starts gets the port, the others say who has
it. The agent sees in the answer to `initialize` (`serverInfo`) and with the `status` tool which program it talks to.

```
agent  --HTTP-->  http://127.0.0.1:7777/mcp  =  editor (open scene, live)  or  AtlasBridge.exe mcp (scene files)
```

The reference of jobs, queries and edit operations is `src/bridge/README.md` (`docs` tool).

## 1. Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DATLAS_EDITOR=ON -DATLAS_BRIDGE=ON
cmake --build build --target AtlasEngineEditor AtlasBridge --parallel
```

## 2. Start a program that offers the server

- **Editor**: open a project (project hub, or `AtlasEngineEditor.exe --project D:\Repos\MyGame\MyGame.aeproject`). The
  log says `MCP server of the editor on http://127.0.0.1:7777/mcp`, a notification says when another program has the
  port. `--mcp-port <port>` uses another port.
- **Without the editor**: `AtlasBridge.exe mcp [--port 7777]` in the project directory, it runs until it is closed.

## 3. Connect the agent

The repositories contain a `.mcp.json` with the URL, Claude Code (terminal and the Code tab of the desktop app) offers
the `atlas` server when a session starts in the repository; approve it once, `/mcp` shows its state. Elsewhere:

```powershell
claude mcp add --transport http atlas http://127.0.0.1:7777/mcp
```

**Codex** (`~/.codex/config.toml`): `[mcp_servers.atlas]` with `url = "http://127.0.0.1:7777/mcp"`.

**Claude Desktop chat** only starts local servers itself (stdio), a shim like `npx mcp-remote http://127.0.0.1:7777/mcp`
as the command connects it.

**ChatGPT** connects from the servers of OpenAI and needs a public HTTPS address: start the program with a token
(`ATLAS_BRIDGE_TOKEN`, or `AtlasBridge.exe mcp --token <secret>`), expose the port with a tunnel, e.g.
`cloudflared tunnel --url http://127.0.0.1:7777`, and add `https://<tunnel>/mcp` with the token as bearer token.
Anyone with the address and the token can run Lua in your editor, stop the tunnel when you are done.

## 4. Check the connection

Ask the agent "which program and scene is open, and what is selected?". It calls `status`, and in the editor `lua`:

```lua
Bridge.Result("entities", #Bridge.Find("*"))
Bridge.Result("selection", #Bridge.Selection())
Bridge.Result("camera", Bridge.Camera())
```

`screenshot` shows the editor window as you see it.

## Tools

| Tool | Editor | AtlasBridge.exe |
|---|---|---|
| `lua` | the open scene, one undo step (Ctrl+Z), the scene is marked as changed and saved only by you; `Project` changes are undone and saved by you in the project settings; `Editor` lists, opens and focuses windows and reads or changes code windows | the scene the bridge server has loaded (`eval`) |
| `status` | project, scene, playing, selection | the bridge server and the programs on the GPU |
| `screenshot`, `undo` | the editor window, undo or redo the last step | - |
| `run`, `check`, `inspect`, `job_status`, `cancel` | jobs on scene files, run by AtlasBridge.exe next to the editor | the same |
| `view_image`, `docs` | images of jobs, the reference | the same |
| `bindings` | the Lua bindings with their types, a filter or annotations for the Lua language server | the same |

A tool call runs on the main thread of the editor between two frames, a call takes about one frame.

## Troubleshooting

- The client can't connect: no program offers the server (editor without a project, or nothing started).
- "Port 7777 is used by atlas-bridge": the bridge was started first, close it or start the editor with `--mcp-port`.
- Changes while the scene plays have no undo step, they are discarded when playing stops like every change while playing.
