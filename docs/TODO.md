# Status

Usable, and used, but the version number is honest: interfaces still move.

## Known gaps

- The registry code assumes a little-endian host
- `support/commandline.h` is 900 lines of inline code, paid for by every translation unit that
  includes it

## Wanted

- Mutually exclusive option groups for `cli`, so that `--json` and `--xml` can rule each other
  out. SysCmdLine's version of this interacts with its option priority ladder, so decide what
  the semantics should be rather than copying its shape.

## Unverified

- The `sigpipe_guard` in `popen_unix.cpp` covers the gap between `poll()` saying a descriptor is
  writable and the write happening, and no test reaches it. The poll loop answers every broken
  pipe a test can arrange by not writing at all, so taking the guard away leaves the suite green,
  measured on Linux and macOS, and 900 rounds of a child exiting at once against a megabyte of
  input never hit the gap either. It is kept on the argument that a library should not end the
  process it is embedded in, however rarely, and it costs two system calls per `communicate()`.
- `console::width()` reads a Windows console through `GetConsoleScreenBufferInfo`. That branch
  was measured by hand against a real console and reported 120 columns, but no test covers it:
  the POSIX side makes a pty to ask, and the Windows equivalent is a pseudoconsole and a second
  process.
