# chest

Old-school (bitboards, negamax+pruning) chess engine, with the following goals:

- Learn modern C++
- Be fast (hence, along with the last point, the heavy use of concepts/class templates and
  disdain for anything dynamic)
- Deep dive on alpha-beta search (all about move ordering, aspiration windows,
  heuristics)
- Write a good static eval, and maybe train a better eval at some point

I.e. this is mostly built for me to learn, and hence I am not being judicious in my selection of what to include at all!
Since it is in the early stages, (except for IO type functionality) the main library `libChess` is header-only. This will probably change once the headers themselves are a bit more stable. So much of it is `constexpr` though, so maybe not, we'll see.

# Features

- Bitboard board representation, compact (16-bit, bitflag heavy) move representation,
- Parse FEN strings,
- Pseudo-legal movegen:
    - Sliding pieces: plain (fixed-shift) magic bitboards
    - Look up tables for jumping pieces
- Staged move generation: loud (tactical) vs. quiet moves
- Make/unmake move (> 25Mn/s on perft):
    - Single threaded,
    - Including legality check,
    - No bulk-counting

# TODO

Lots to do, here's the short term roadmap. This is where we're at:

```
$ perf stat -d ./perft test

...
 Performance counter stats for './perft_test':
    TOTAL (LEGAL) NODES: 707 million
    AVERAGE RATE: 35.1322Mn/s
===============================================================================
All tests passed (54 assertions in 1 test case)


 Performance counter stats for './perft_test':

         20,507.82 msec task-clock:u                     #    0.999 CPUs utilized
                 0      context-switches:u               #    0.000 /sec
                 0      cpu-migrations:u                 #    0.000 /sec
               731      page-faults:u                    #   35.645 /sec
   239,505,392,974      instructions:u                   #    3.27  insn per cycle              (62.49%)
    73,234,310,745      cycles:u                         #    3.571 GHz                         (74.98%)
    30,183,258,260      branches:u                       #    1.472 G/sec                       (74.99%)
       180,319,418      branch-misses:u                  #    0.60% of all branches             (74.98%)
    67,605,972,873      L1-dcache-loads:u                #    3.297 G/sec                       (74.98%)
        23,653,623      L1-dcache-load-misses:u          #    0.03% of all L1-dcache accesses   (75.02%)
         1,287,154      LLC-loads:u                      #   62.764 K/sec                       (50.03%)
           217,257      LLC-load-misses:u                #   16.88% of all LL-cache accesses    (50.00%)

      20.528823266 seconds time elapsed

      20.448400000 seconds user
       0.006587000 seconds sys
```

So, movegen is looking pretty good, but hopefully making those magic bitboards smaller will help with L3 cache pressure.
For movegen:

* Make bitboards more compact
* Simplify make/unmake move
  (express in terms of atomic piece movement, to make incremental state/augmented state (occupancy)/Zobrist key/eval easy)

Then, the gameplan for the rest of the engine:

## Eval

* Add piece-value eval heuristics
* Maybe some terms for open files, passed pawns, early/late game king position, etc.
* Maybe mess around with some NNUE, if I get to it

## Search

* Implement alpha-beta pruning negamax
* Add transposition tables/zobrist hashing
* Add aspiration windows
* Add full PV search

## General engine

* Add UCI support
