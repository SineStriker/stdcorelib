# Status

Usable, and used, but the version number is honest: interfaces still move.

## Known gaps

- The registry code assumes a little-endian host
- A detached child is only collected the next time another one is started, so a process that detaches many and spawns nothing else keeps them in the process table until it exits
- `winextra.cpp` carries two functions marked for review
