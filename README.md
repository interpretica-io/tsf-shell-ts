# tsf-shell-ts

A Test Environment suite that exercises
[tsf-shell](https://github.com/interpretica-io/tsf-shell) (`tapi_shell`)
against the agent it runs on — an interactive TTY and a rendered
terminal screen, with no mock and no network (the terminal emulator is a
library linked into the agent's RPC server). Every test produces its own
deterministic terminal output on the agent and asserts tsf-shell reads it
back correctly.

| Test | What it checks |
|---|---|
| `render` | a shell paints a known screen with cursor-positioning escapes; each row reads back exactly (`LOGIN:`, `username`, `password`) |
| `cursor` | a `CSI row;col H` positioning sequence puts the cursor at the cell tsf-shell then reports, over several targets |
| `attrs` | SGR colour/attribute sequences give cells with the right palette index (ANSI 30-37 → 0-7), background and bold/underline/reverse flags |
| `expect` | an interactive echo shell is driven: input is typed, the echo is awaited on the output **stream** (classic expect), and the same text is found on the rendered **screen** with its cell coordinates |
| `resize` | resizing reaches the child — its `stty size` changes from `24 80` to `40 100` (pty winsize + SIGWINCH) — and the emulator's own screen size follows |
| `wait` | a spawned program's exit code is reported (0, 3, 7, 42), and a process that `kill -TERM`s itself is reported as signalled |
| `conformance` | a fixed escape script is fed in and the whole rendered grid is compared to a golden expectation — a programmatic vttest, with the emulator as the DUT |
| `erase` | erase-in-line / erase-in-display (EL/ED) clear exactly the cells the escape names and no others |
| `scroll` | a scrolling region (DECSTBM) confines scrolling to a band of rows; a row outside it stays put |
| `edit` | in-line editing — insert-character (ICH) and delete-character (DCH) — rearranges a row, interior blanks and all |
| `tabs` | horizontal tabs land on the default stops (every 8) and on a custom stop set with HTS after clearing all with TBC |
| `wrap` | autowrap (DECAWM) carries a line past the last column onto the next row on a narrow screen |
| `altscreen` | the alternate screen buffer (DECSET 1049) hides the primary content on enter and restores it exactly on leave |
| `unicode` | UTF-8 is decoded to code points; a wide CJK glyph occupies two cells and is flagged wide, the next letter follows it |
| `savecursor` | save-cursor / restore-cursor (DECSC/DECRC) park the cursor, detour to print, and restore to the parked cell |
| `color` | a 24-bit direct-colour SGR sets an exact `0xRRGGBB` fg/bg, and a 256-colour SGR sets a palette index |
| `cursorvis` | cursor-visibility (DECTCEM, `CSI ?25h`/`l`) is tracked and reported |
| `keys` | symbolic keys become the exact xterm byte sequences (arrows, Home, F1, Delete, PageUp, Alt-modified), read back in raw mode |
| `signals` | a control key becomes a signal — Ctrl-C → SIGINT, Ctrl-`\` → SIGQUIT — terminating the foreground reader |
| `scrollback` | lines scrolled off the top of the screen are kept in scrollback and found there, absent from the visible screen |
| `rowattrs` | a whole row's per-cell attribute records (column, code point, colours, indices, flags) are dumped and parsed |
| `regex` | the expect primitive matches a POSIX extended regex on the stream, and reports `ETIMEDOUT` for a pattern that never arrives |
| `multisession` | several terminal sessions live at once on one agent, each with its own screen, and do not bleed into one another |

A terminal-emulator backend is **required**; a test skips
(`tapi_shell_available()`) when none is linked into the agent, rather
than failing. The Docker image installs **libvterm**, the fallback
backend; the preferred libghostty-vt needs a Zig build and is not
installed here. Numeric results are emitted as MI measurement artifacts,
so the HTML log and Bublik draw them as graphs (characters per row,
colour index per cell, echo round-trip latency, screen size across the
resize, exit codes, golden cells matched).

## Oracle

Nothing is committed and nothing runs on the DUT but the program each
test spawns. The ground truth is the script the test writes: a shell
printing fixed escape sequences (`render`, `attrs`, `conformance`),
reading and echoing (`expect`), reporting its tty size (`resize`), or
just exiting with a code (`wait`). The emulation is driven in-process in
the agent's RPC server, so every byte is rendered locally and
deterministically. A host with no terminal-emulator backend skips the
suite cleanly.

## Running it

Needs Docker and `test-environment` as a sibling directory:

```bash
git clone https://github.com/interpretica-io/test-environment ../test-environment
./scripts/run.sh docker guess --cfg=localhost
```

The Docker image (`scripts/docker/build/Dockerfile`) carries the TE
build dependencies and **libvterm-dev**, so the agent — the build
container on the `localhost` configuration — has a backend to render
with. Without Docker: `./scripts/run.sh guess --cfg=localhost` on a host
that has the TE dependencies and `libvterm-dev`.

Check results from the build directory: `cat log.txt` for per-iteration
`PASSED`/`FAILED`, and `./scripts/html-log.sh` for the HTML report with
the MI graphs.

## Pointing it at a real host

On `localhost` the agent is the build container, so the "DUT" is that
container. Name a host in `conf/rcf.conf` and the same tests drive a TTY
on that host instead — a serial console or a device's shell — which is
where `tapi_shell_attach` (serial/pty/socket) and the emulator-as-DUT
conformance test earn their keep.
