# chest

Old-school C++ chess engine. Play it on [lichess](https://lichess.org/?user=chestbot#friend)!

# Features

The engine supports:

- Bitboards
- PEXT/magic pseudo-legal staged movegen w/legality checks (>35Mn/s perft)
- Make/unmake-style traversal
- Incrementally updated PST eval/Zobrist hashes
- (Partial) UCI suport

Library design:

- Header-only (except IO) core engine (`libChess`)
  - Templates/static polymorphism favoured (not a vtable in sight except for logging)
  - See e.g. `makemove.h` and `incremental.h`
- General-purpose "frontend" design (see `engine.h`) with UCI implementation (see `UCI.h`)
  - May support other protocols

# Build

If on x64: Run `cmake --build build/release-native` to build the release version.
On other platforms: remove or modify `CmakePresets.json` and build.

Uses catch2 for tests.

# TODO

Short term goals:

- [ ] Quiescence search
- [ ] Transposition tables
- [ ] MVV/LVA ordering
- [ ] Null-window/PV search
- [ ] Aspiration windows
- [ ] Full UCI compliance

Long term goals:

- [ ] SEE ordering
- [ ] Killer/history heuristic ordering
- [ ] NNUEs
