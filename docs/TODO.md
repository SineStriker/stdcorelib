# Status

Usable, and used, but the version number is honest: interfaces still move.

## Known gaps

- The registry code assumes a little-endian host
- `support/commandline.h` is 900 lines of inline code, paid for by every translation unit that
  includes it
- `test_close_fds` in `tests/auto/support/test_popen.cpp` counts what a child has open by
  listing `/proc/self/fd`, which macOS does not have. It passes there without checking anything.

## Wanted

- Mutually exclusive option groups for `cli`, so that `--json` and `--xml` can rule each other
  out. SysCmdLine's version of this interacts with its option priority ladder, so decide what
  the semantics should be rather than copying its shape.

## Unverified

- `console::width()` reads a Windows console through `GetConsoleScreenBufferInfo`. That branch
  was measured by hand against a real console and reported 120 columns, but no test covers it:
  the POSIX side makes a pty to ask, and the Windows equivalent is a pseudoconsole and a second
  process.
