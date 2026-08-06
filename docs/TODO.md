# Status

Usable, and used, but the version number is honest: interfaces still move.

## Known gaps

- The registry code assumes a little-endian host
- `cli::ParseResult::value<T>()` answers with a value initialized `T` when the conversion fails.
  `tryValue()` is beside it for callers who need to tell that apart from a real zero.
- `support/commandline.h` is 900 lines of inline code, paid for by every translation unit that
  includes it

## Wanted

- Mutually exclusive option groups for `cli`, so that `--json` and `--xml` can rule each other
  out. SysCmdLine's version of this interacts with its option priority ladder, so decide what
  the semantics should be rather than copying its shape.

## Unverified

- `test_threads` in `tests/auto/support/test_popen.cpp` was written for a SIGPIPE defect that
  only ever appeared on macOS, and has never been run there. What it is worth is whether it goes
  red with the `SIG_IGN` in `sigpipe_guard` put back to blocking the signal. Neither Linux nor
  Windows can answer that, since both survived the defect.
- `console::width()` reads a Windows console through `GetConsoleScreenBufferInfo`. That branch
  was measured by hand against a real console and reported 120 columns, but no test covers it:
  the POSIX side makes a pty to ask, and the Windows equivalent is a pseudoconsole and a second
  process.
