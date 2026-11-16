# tsf-shell-ts

A Test Environment (TE) test suite for
[tsf-shell](https://github.com/interpretica-io/tsf-shell). It tests the
interactive terminal and the rendered screen on the agent it runs on.
Nothing is mocked and no network is used: the terminal emulator is a
library inside the agent's RPC server. Each test makes the terminal show
something known in advance and checks that tsf-shell reads it back.

| Test | What it checks |
|---|---|
| `render` | A shell draws a known screen with cursor-positioning sequences; each row reads back exactly (`LOGIN:`, `username`, `password`) |
| `cursor` | `CSI row;col H` puts the cursor in the cell that tsf-shell then reports, for several positions |
| `attrs` | SGR sequences give cells the right palette index (ANSI 30-37 are 0-7), background, and bold, underline and reverse flags |
| `expect` | Input is typed into an echoing shell; the echo is found in the output stream and on the screen, with its cell position |
| `resize` | A resize reaches the child (`stty size` changes from `24 80` to `40 100`) and the emulator's screen size follows |
| `wait` | The exit codes 0, 3, 7 and 42 are reported, and a process that kills itself with SIGTERM is reported as signalled |
| `conformance` | A fixed escape script is fed in and the whole screen is compared with the expected grid |
| `erase` | Erase in line and erase in display (EL, ED) clear exactly the cells they name |
| `scroll` | A scrolling region (DECSTBM) limits scrolling to its rows; a row outside it does not move |
| `edit` | Insert character and delete character (ICH, DCH) shift the rest of the row, blanks included |
| `tabs` | Tabs stop at the default positions (every 8 columns) and at a custom stop set with HTS after TBC clears all |
| `wrap` | Autowrap (DECAWM) continues a line on the next row when it passes the last column |
| `cbtwrap` | A back-tab (CBT) from the last column cancels the pending wrap, as cursor-left does |
| `altscreen` | The alternate screen (DECSET 1049) hides the main screen and restores it exactly on exit |
| `unicode` | UTF-8 decodes to code points; a wide CJK character takes two cells and is marked wide; the next letter follows it |
| `savecursor` | Save and restore cursor (DECSC, DECRC) return the cursor to the saved cell after printing elsewhere |
| `color` | A 24-bit SGR colour gives an exact `0xRRGGBB` foreground and background; a 256-colour SGR gives a palette index |
| `cursorvis` | Cursor visibility (DECTCEM, `CSI ?25h` and `CSI ?25l`) is tracked and reported |
| `keys` | Keys become the xterm byte sequences (arrows, Home, F1, Delete, PageUp, Alt combinations), read back in raw mode |
| `signals` | Ctrl-C sends SIGINT and Ctrl-`\` sends SIGQUIT to the program reading the terminal |
| `scrollback` | Lines that scroll off the top are in the scrollback and not on the screen |
| `deepscroll` | The scrollback holds exactly the newest 2000 lines, in order and without gaps, for line counts below, at and above that limit |
| `rowattrs` | The cell records of a row (column, code point, colours, indices, flags) are returned and parsed |
| `regex` | `expect` matches a POSIX extended regex in the stream and returns `ETIMEDOUT` for a pattern that never appears |
| `multisession` | Several sessions run at once on one agent, each with its own screen, without affecting each other |
| `chunking` | A terminal shows the same thing whether its input arrives in one write or one byte at a time, for 23 sequences that change its state (pending wrap, margins, modes, split escape sequences and UTF-8) |
| `snapshot` | A terminal restored from its own snapshot shows the same thing and renders further output the same way, for the same 23 sequences (libghostty-vt only) |

Every test has a `backend` parameter and runs once with libghostty-vt
and once with libvterm, so a difference between the two emulators is a
difference between two iterations of the same test. If the agent does
not have a backend, its iterations are skipped. The Docker image has
both: libvterm from Debian, and libghostty-vt built from a pinned
Ghostty commit and linked statically into the agent.

`chunking`, `snapshot` and `deepscroll` write their input directly into
the emulator (`tapi_shell_feed`) while the session runs an idle program,
so their results do not depend on the pty or on timing.

Numeric results are recorded as TE MI measurements, which the HTML log
shows as graphs.

## Expected values

The expected result of each test is fixed by the test itself: a shell
script that prints known escape sequences (`render`, `attrs`,
`conformance`), echoes its input (`expect`), prints its terminal size
(`resize`) or exits with a known code (`wait`), or bytes the test feeds
in directly. The tests change nothing on the agent except the processes
they start.

## Running

You need Docker, and a clone of Test Environment next to the suite:

```bash
git clone https://github.com/interpretica-io/test-environment ../test-environment
./scripts/run.sh docker guess --cfg=localhost 2>&1 | tee run.out
scripts/check-run.sh run.out
```

The Docker image (`scripts/docker/build/Dockerfile`) contains the TE
build dependencies and both backends. With the `localhost`
configuration the agent runs in the same container. The first image
build compiles libghostty-vt; this takes several minutes and needs
network access, and later builds reuse the cached layer.

Without Docker, run `./scripts/run.sh guess --cfg=localhost` on a host
that has the TE dependencies and at least one backend that pkg-config
can find (`libghostty-vt-static` or `vterm`).

To build against a local copy of tsf-shell, set its `url` in
`conf/external.yml` to the path of the copy. To build from a private
fork, set `GH_TOKEN` to a token that can read it: `run.sh` passes the
token to git in the container through a credential helper, by variable
name, so it does not appear in any command line or URL.

## Checking the result

`run.sh` exits with 0 whenever the run finishes, whatever the results.
`scripts/check-run.sh` reads the Tester's summary from the saved output
and fails unless every iteration passed or failed as `conf/trc.xml`
expects: nothing unexpected, aborted, new or skipped unexpectedly.

A full run has 123 iterations. 120 pass and 3 fail as expected, because
of known defects in the emulators:

| Test | Backend | Known defect |
|---|---|---|
| `cbtwrap` | libghostty-vt | A back-tab (CBT) from the last column keeps the pending wrap, so the next character goes to the row below the cursor |
| `snapshot`, `seq=cbt_pending` | libghostty-vt | The same defect after a snapshot: the snapshot drops the flag, so the restored terminal puts that character somewhere else |
| `chunking`, `seq=split_utf8` | libvterm (0.1.4 in the image; 0.3.3 too) | A UTF-8 character split between two writes becomes U+FFFD |

Each entry includes the verdict of that defect. A different failure of
the same test is still reported as unexpected, and a fixed defect is
reported as "passed unexpectedly"; then remove the entry.

Details of each iteration are in `log.txt`; `./scripts/html-log.sh`
builds the HTML report with the graphs.

## Pinned versions

| What | Where |
|---|---|
| tsf-shell | `conf/builder.conf.lock`: the commit the suite last passed with. `dispatcher.sh --update-external` updates it |
| Ghostty (libghostty-vt) | `GHOSTTY_REF` in `scripts/docker/build/Dockerfile` |
| Zig | `ZIG_VERSION` and its SHA-256 for each architecture, in the same file |

The image build checks the Zig archive against the checksum and the
Ghostty commit against `GHOSTTY_REF`. The CI of tsf-shell builds the
same Ghostty commit (`ci/ghostty.env` there); change both together.

## Development

`scripts/lint.sh` checks layout and licence headers, XML syntax, and
runs shellcheck and actionlint. It also runs `scripts/lint_suite.py`,
which checks that the parts of the suite agree with each other:

- every test C file is built by meson and run by `package.xml`;
- every run has the `backend` parameter, and the `seq` values match the
  sequences in `lib/tsapi_shell.c`;
- every expected result in `conf/trc.xml` names a test that exists, the
  parameter values it runs with, and a verdict its source can produce;
- every test has a row in the table above.

GitHub Actions (`.github/workflows/ci.yml`) runs `scripts/lint.sh` on
every push and pull request. The full Docker build and run starts by
hand or once a week, because Test Environment is taken from its main
branch and changes independently. That job checks the result with
`scripts/check-run.sh` and keeps the logs as an artifact.

## Running on another host

With `localhost`, the agent is the build container. Name another host in
`conf/rcf.conf` and the same tests use a terminal on that host. There,
`tapi_shell_attach` can also read a serial console or a device shell.
