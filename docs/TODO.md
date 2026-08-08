# Status

Usable, and used, but the version number is honest: interfaces still move.

## Before a release

- Port qmsetup's `corecmd` onto `cli`. It is the one consumer this was written for, and until it builds against it the API has been validated by nothing but its own tests.
- Nothing has ever been tagged, and there is no changelog. Both wait on a stretch with no breaking change in it, which the reader API of `ParseResult` and the signature of `VersionNumber::fromString` have not had yet.
- The installed package config says `AnyNewerVersion`, which promises that 0.0.2 answers a request for 0.0.1. Under semver a 0.x minor is the breaking position, so that promise is wrong until 1.0. The soname already treats it as breaking.

## Known gaps

- The registry code assumes a little-endian host
- `support/commandline.h` is over a thousand lines of inline code, paid for by every translation unit that includes it

## Wanted

- Mutually exclusive option groups for `cli`, so that `--json` and `--xml` can rule each other out. SysCmdLine's version of this interacts with its option priority ladder, so decide what the semantics should be rather than copying its shape.
- `communicate()` on **Windows** starts one worker thread per open pipe, and with a single pipe there is nothing to interleave with, so the thread is only there to make a timeout interruptible. CPython skips it in that case (`Lib/subprocess.py:1199`, at most one pipe and no timeout). POSIX here has nothing to fix: it is one `poll()` loop and no threads, which is what CPython does on that side too. Probably not worth doing at all, since a thread costs tens of microseconds against the milliseconds of `CreateProcessW` beside it, and it buys a second path through the one function in this library whose deadlock reasoning is subtle. Measure before writing it.

## Unverified

- The gap the `sigpipe_guard` in `popen_unix.cpp` covers, between `poll()` saying a descriptor is writable and the write happening, cannot be arranged from a test. That the guard is installed for the length of a `communicate()` and puts back what it found is checked, by sampling the disposition from another thread while one runs. What is not checked is that the gap itself is survivable, and 900 rounds of a child exiting at once against a megabyte of input never produced one.
- `console::width()` reading a Windows console is checked against the console the suite is attached to, and skipped where there is none. That it reads the visible window rather than the scrollback buffer is not: Windows Terminal gives the buffer the same width as the window, so reading `dwSize.X` instead passes, measured. It would fail on a console whose buffer somebody widened, which is the case the code is written for.
