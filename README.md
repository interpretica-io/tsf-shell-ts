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
| `cbtwrap` | a back-tab (CBT) from the last column cancels the pending wrap, like cursor-left does |
| `altscreen` | the alternate screen buffer (DECSET 1049) hides the primary content on enter and restores it exactly on leave |
| `unicode` | UTF-8 is decoded to code points; a wide CJK glyph occupies two cells and is flagged wide, the next letter follows it |
| `savecursor` | save-cursor / restore-cursor (DECSC/DECRC) park the cursor, detour to print, and restore to the parked cell |
| `color` | a 24-bit direct-colour SGR sets an exact `0xRRGGBB` fg/bg, and a 256-colour SGR sets a palette index |
| `cursorvis` | cursor-visibility (DECTCEM, `CSI ?25h`/`l`) is tracked and reported |
| `keys` | symbolic keys become the exact xterm byte sequences (arrows, Home, F1, Delete, PageUp, Alt-modified), read back in raw mode |
| `signals` | a control key becomes a signal — Ctrl-C → SIGINT, Ctrl-`\` → SIGQUIT — terminating the foreground reader |
| `scrollback` | lines scrolled off the top of the screen are kept in scrollback and found there, absent from the visible screen |
| `deepscroll` | the scrollback keeps exactly the newest 2000 scrolled-off lines, in order and without gaps, for line counts below, at and above that limit |
| `rowattrs` | a whole row's per-cell attribute records (column, code point, colours, indices, flags) are dumped and parsed |
| `regex` | the expect primitive matches a POSIX extended regex on the stream, and reports `ETIMEDOUT` for a pattern that never arrives |
| `multisession` | several terminal sessions live at once on one agent, each with its own screen, and do not bleed into one another |
| `chunking` | a terminal looks the same whether its input arrives in one write or one byte at a time, over a set of state-setting sequences (pending wrap, margins, modes, cut escape sequences and UTF-8) |
| `snapshot` | a terminal replaced by the one decoded from its own snapshot looks the same and renders the same further output, over the same set of sequences (libghostty-vt) |

Every test takes the emulator as a parameter and runs once per backend,
**libghostty-vt** and **libvterm**, so a difference between the two
shows as a difference between two iterations of one test. A test skips
when the agent lacks the backend of its iteration, rather than failing.
The Docker image carries both: libvterm from the distribution, and
libghostty-vt built from a pinned Ghostty commit with Zig and linked
statically into the agent.

`chunking` and `snapshot` drive the emulator itself: the session runs an
idle program and the test feeds the bytes straight into the emulator
(`tapi_shell_feed`), so nothing depends on the pty or on timing.

Numeric results are emitted as MI measurement artifacts, so the HTML log
and Bublik draw them as graphs (characters per row, colour index per
cell, echo round-trip latency, screen size across the resize, exit
codes, golden cells matched, lines differing between two terminals).

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
GH_TOKEN=$(gh auth token) ./scripts/run.sh docker guess --cfg=localhost
```

tsf-shell is a private repository, so the Builder needs credentials to
clone it: `GH_TOKEN` (any token that can read the repository) is handed
to git inside the container by name and never appears in a command line
or a URL. To build against a local checkout of tsf-shell instead, point
its `url` in `conf/external.yml` at the checkout's path.
`conf/builder.conf.lock` pins the tsf-shell commit the suite was last
verified with; `--update-external` moves it on.

The Docker image (`scripts/docker/build/Dockerfile`) carries the TE
build dependencies and both backends, so the agent — the build
container on the `localhost` configuration — has them to render with.
The first image build compiles libghostty-vt, which takes several
minutes and needs network access; later builds reuse the cached layer.
Without Docker: `./scripts/run.sh guess --cfg=localhost` on a host that
has the TE dependencies and at least one backend installed where
pkg-config finds it (`libghostty-vt-static`, `vterm`).

## Reading the result

A run ends with the Tester's summary. `run.sh` exits 0 whenever the run
completes, whatever the tests did, so keep its output and let
`scripts/check-run.sh` decide:

```bash
GH_TOKEN=$(gh auth token) ./scripts/run.sh docker guess --cfg=localhost 2>&1 | tee run.out
scripts/check-run.sh run.out
```

It passes when every iteration passed or failed as `conf/trc.xml`
expects — none unexpected, aborted, new or skipped unexpectedly. A full
run is 123 iterations: 120 pass, and 3 fail as expected on known
defects of the emulators:

| Test | Backend | Known defect |
|---|---|---|
| `cbtwrap` | libghostty-vt | A back-tab (CBT) from the last column keeps the pending wrap: the next character lands one row below the cursor |
| `snapshot`, `seq=cbt_pending` | libghostty-vt | The same defect seen through a snapshot: the snapshot drops the flag, so the restored terminal prints that character elsewhere |
| `chunking`, `seq=split_utf8` | libvterm (0.1.4 in the image, also 0.3.3) | A UTF-8 character cut between two writes decodes as U+FFFD |

Each entry carries the verdict that identifies the defect, so a
different failure of the same test still shows as unexpected, and a fix
shows as "passed unexpectedly" — then drop the entry. Per-iteration
detail is in `log.txt`, and `./scripts/html-log.sh` gives the HTML
report with the MI graphs.

## Versions

Everything the run depends on is pinned, so a run can be repeated:

| What | Where |
|---|---|
| tsf-shell | `conf/builder.conf.lock` (the commit last verified; `--update-external` moves it on) |
| Ghostty (libghostty-vt) | `GHOSTTY_REF` in `scripts/docker/build/Dockerfile` |
| Zig | `ZIG_VERSION` and its published SHA-256 per architecture, same file |

The image build checks the Zig archive against its checksum and the
fetched Ghostty commit against `GHOSTTY_REF`. tsf-shell's own CI builds
the same Ghostty commit (`ci/ghostty.env` there); move both together.

## Development

`scripts/lint.sh` runs the static checks: layout and licence headers,
well-formed XML, shellcheck, actionlint, and `scripts/lint_suite.py`,
which checks that the suite's descriptions of itself agree — every test
C file is built by meson and run by `package.xml`; every run takes the
`backend` argument; the `seq` values are the sequences in
`lib/tsapi_shell.c`; every expected result in `conf/trc.xml` names a
test, the arguments it is run with and a verdict its source can
produce; and every test has a row in the table above.

GitHub Actions (`.github/workflows/ci.yml`) runs the lint on every push
and pull request, and the full Docker build and run on demand and
weekly (Test Environment is taken from its main branch and moves on its
own). The full run needs a token that can read tsf-shell in the
`TSF_SHELL_TOKEN` repository secret; it keeps the logs as an artifact.

## Pointing it at a real host

On `localhost` the agent is the build container, so the "DUT" is that
container. Name a host in `conf/rcf.conf` and the same tests drive a TTY
on that host instead — a serial console or a device's shell — which is
where `tapi_shell_attach` (serial/pty/socket) and the emulator-as-DUT
conformance test earn their keep.
