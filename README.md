# chest

Old-school (bitboards, negamax+pruning) chess engine, with the following goals:

- Learn modern C++
- Be fast (hence, along with the last point, the heavy use of concepts/class templates and
  disdain for anything dynamic)
- Deep dive on alpha-beta search (all about move ordering, aspiration windows,
  heuristics)
- Write a good linear eval, and maybe train a better eval at some point

I.e. this is mostly built for me to learn, and hence I am not being judicious in my selection of what to include at all!
Since it is in the early stages, (except for IO type functionality) the main library `libChess` is header-only. This will probably change once the headers themselves are a bit more stable. So much of it is `constexpr` though, so maybe not, we'll see.

# Features

- Bitboard board representation, compact (16-bit, bitflag heavy) move representation,
- Parse FEN strings,
- Magic bitboard sliding piece pseudo-legal move generation,
- Precomputed jumping piece move generation,
- Castling,
- Staged move generation: loud (tactical) vs. quiet moves

# TODO

## Move Generation

- Check detection (legal move generation)
- Capture/check first move ordering

Further:

- Check evasion only

## Move making

- Zobrist hashing
- Make/unmake move

## Evaluation

- Material evaluation
- King safety
- Pawn structure

Possibly:

- Incremental update (store with state/search node)

## Search

- Id-dfs
- Alpha/beta pruning (fail soft/hard)
- Halfmove clock

Further:

- Quiescence search
- LMR
- Endgame tablebase
- Opening book

## Engine

- Support a standard (can't be bothered to build a frontend)
