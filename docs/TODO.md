# Status

Usable, and used, but the version number is honest: interfaces still move.

## Known gaps

- The registry code assumes a little-endian host
- `support/commandline.h` is over a thousand lines of inline code, paid for by every translation
  unit that includes it

## Wanted

- Mutually exclusive option groups for `cli`, so that `--json` and `--xml` can rule each other
  out. SysCmdLine's version of this interacts with its option priority ladder, so decide what
  the semantics should be rather than copying its shape.

## Unverified

- The gap the `sigpipe_guard` in `popen_unix.cpp` covers, between `poll()` saying a descriptor is
  writable and the write happening, cannot be arranged from a test. That the guard is installed
  for the length of a `communicate()` and puts back what it found is checked, by sampling the
  disposition from another thread while one runs. What is not checked is that the gap itself is
  survivable, and 900 rounds of a child exiting at once against a megabyte of input never
  produced one.
- `console::width()` reading a Windows console is checked against the console the suite is
  attached to, and skipped where there is none. That it reads the visible window rather than the
  scrollback buffer is not: Windows Terminal gives the buffer the same width as the window, so
  reading `dwSize.X` instead passes, measured. It would fail on a console whose buffer somebody
  widened, which is the case the code is written for.
