# Rchess

C++ chess engine written by a guy who doesn't know C++.

After a failed attempt in R (all I want is to stack allocate a uint64_t, is that too much to ask?), hence the name.

# Features

* Bitboard board representation
* Parse FEN string
* Magic bitboard sliding piece pseudo-legal move generation

# TODO

## Move Generation

* Jumping piece pseudo-legal move generation
* Castling
* Check detection (legal move generation)
* Capture/check first move ordering

Further:

* Loud line generation
* Check evasion only

## Move making

* Zobrist hashing
* Make/unmake move

## Evaluation

* Material evaluation
* King safety
* Pawn structure

Possibly:

* Incremental update (store with state/search node)

## Search

* Id-dfs
* Alpha/beta pruning (fail soft/hard)
* Halfmove clock

Further:

* Quiescence search
* LMR
* Endgame tablebase
* Opening book

## Engine

* Support a standard (can't be bothered to build a frontend)
