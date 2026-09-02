# Bitboard System & Move Generation Documentation

This document explains the bitboard architecture, lookup tables, magic bitboard initialization, and attack-generation API used by the engine. It is written so that anyone can understand both high-level concepts and exact implementation details without prior engine-programming experience.

---

## 1. Core Concepts & Predefined Masks

### What is a Bitboard?
A **Bitboard** is a 64-bit unsigned integer (`uint64_t`) where every bit directly corresponds to one of the 64 squares on a chessboard.
* A bit set to `1` means a piece or target square is active.
* A bit set to `0` means the square is empty or unselected.

### File Masks (`FileABB` through `FileHBB`)
* **What they represent:** Entire vertical columns on the board.
* `FileABB` (`0x0101010101010101ULL`) has a `1` on every square along the A-file (`A1, A2, ..., A8`).
* Shifting `FileABB` left by 1 to 7 columns generates masks for files B through H.

### Rank Masks (`Rank1BB` through `Rank8BB`)
* **What they represent:** Entire horizontal rows on the board.
* `Rank1BB` (`0xFFULL` = 8 continuous `1` bits) selects all squares on the 1st rank (`A1` through `H1`).
* Shifting by multiples of 8 bits produces ranks 2 through 8.

---

## 2. Magic Bitboard Lookup Engine (`struct Magic`)

Calculating moves for sliding pieces (Rooks, Bishops, and Queens) during search is expensive because pieces on the board block further movement. Step-by-step raycasting inside search loops creates major performance bottlenecks.

**Magic Bitboards** solve this by transforming board blockers into a unique array index in constant time ($O(1)$):

```text
[Current Board Occupancy] & [Attack Path Mask]
                     ↓
[pext64 Hardware Extraction]  OR  [Magic Multiplication & Right-Shift]
                     ↓
             [Dense Table Index]
                     ↓
         [Precomputed Attack Bitboard]
```

### Struct Fields & Logic
* **`mask`**: A bitboard of relevant squares that can block ray attacks from a given square, explicitly excluding the outer board edges[cite: 1, 2].  
* **`attacks`**: A pointer referencing the shared, precalculated attack tables (`RookTable` or `BishopTable`)[cite: 1, 2].  
* **`magic` / `shift`**: Arithmetic variables used in software fallback mode when BMI2 hardware instructions (`pext64`) are unavailable[cite: 1, 2].  
* **`index(Bitboard occupied)`**:
  * **With PEXT (`USE_PEXT`)**: Employs the x86 hardware intrinsic `pext64(occupied, mask)` to extract blocker bits directly into a contiguous, dense integer index[cite: 2].  
  * **Without PEXT**: Multiplies the masked occupancy by a pseudo-random multiplier and shifts right to compress the bits into an index: `((occupied & mask) * magic) >> shift`[cite: 2].  
* **`attacks_bb(Bitboard occupied)`**: Looks up and returns the precalculated attack bitboard corresponding to the given blocker pattern[cite: 2].  

---

## 3. Global Geometry Tables
* **`SquareDistance[64][64]`**: Stores the Chebyshev distance (the maximum of the file difference and rank difference) between any two board squares[cite: 1, 2].  
* **`LineBB[64][64]`**: Stores a full, infinite ray passing through two collinear squares across the entire board[cite: 1, 2].  
* **`BetweenBB[64][64]`**: Stores the squares located strictly between two aligned squares, used for check blocks and pin verification[cite: 1, 2].  
* **`RayPassBB[64][64]`**: Stores the ray starting from square `s1`, continuing through square `s2`, and extending to the edge of the board[cite: 1, 2].  
* **`Magics[64][2]`**: Cache-aligned lookup structures for all 64 squares: index `0` for Bishops and index `1` for Rooks[cite: 1, 2].  

---

## 4. Directional Shifting & Coordinate Helpers
* **`square_bb(Square s)`**: Converts a single square index (`0` to `63`) into a single-bit bitboard representation (`1ULL << s`)[cite: 2].  
* **Bitwise Operator Overloads**: Operators `&`, `|`, `^`, `|=`, and `^=` are overloaded for combinations of `Bitboard` and `Square` to allow direct manipulation without explicit conversion calls[cite: 2].  
* **`more_than_one(Bitboard b)`**: Evaluates whether two or more bits are set using the bit-manipulation formula `(b & (b - 1)) != 0`[cite: 2].  
* **`rank_bb(r)` / `file_bb(f)`**: Retrieves the complete 8-bit line bitboard for a specified rank or file[cite: 2].  

### `shift<Direction>(Bitboard b)`
Shifts all active bits on the board in a specific compass direction while preventing wrap-around errors at the board boundaries[cite: 2]:  
* **Eastward shifts (`EAST`, `NORTH_EAST`, `SOUTH_EAST`)**: Clears `FileHBB` prior to shifting so pieces on the H-file do not wrap onto the A-file[cite: 2].  
* **Westward shifts (`WEST`, `NORTH_WEST`, `SOUTH_WEST`)**: Clears `FileABB` prior to shifting so pieces on the A-file do not wrap onto the H-file[cite: 2].  
* **Double vertical shifts (`NORTH + NORTH`, `SOUTH + SOUTH`)**: Supports two-square moves for initial pawn advances[cite: 2].  

### `pawn_attacks_bb<Color>(Bitboard b)`
Simultaneously generates all diagonal pawn capture target squares[cite: 2]:  
* **White**: Shifts the bitboard `NORTH_WEST` and `NORTH_EAST`[cite: 2].  
* **Black**: Shifts the bitboard `SOUTH_WEST` and `SOUTH_EAST`[cite: 2].  

---

## 5. Low-Level Bit Manipulation Utilities
* **`popcount(Bitboard b)`**: Counts the total number of set bits using hardware-accelerated population count instructions[cite: 2].  
* **`lsb(Bitboard b)`**: Determines the square index of the Least Significant Bit (the lowest set bit)[cite: 2].  
* **`msb(Bitboard b)`**: Determines the square index of the Most Significant Bit (the highest set bit)[cite: 2].  
* **`least_significant_square_bb(Bitboard b)`**: Isolates the lowest active bit as a bitboard using two's complement arithmetic (`b & -b`)[cite: 2].  
* **`pop_lsb(Bitboard& b)`**: Identifies the lowest set square and clears it in-place using `b &= b - 1`, returning the square for iterative move generation[cite: 2].  

---

## 6. Table Generation & Initialization Logic
Defines how attack tables and board geometry lookups are precalculated at startup[cite: 1]:  

### Memory Tables (`RookTable` & `BishopTable`)
* **`RookTable[0x19000]`**: Pre-allocated array containing ~102,400 entries to store all blocker permutations across every square for Rooks[cite: 1].  
* **`BishopTable[0x1480]`**: Pre-allocated array containing ~5,248 entries to store all blocker configurations for Bishops[cite: 1].  

### `SimplePRNG`
A deterministic 64-bit SplitMix pseudo-random number generator designed to search for collision-free magic multipliers in environments lacking BMI2 PEXT support[cite: 1].  
* **`sparse_rand64()`**: Bitwise ANDs three consecutive 64-bit random values (`rand64() & rand64() & rand64()`) to generate numbers with sparse bit distributions, reducing indexing collisions[cite: 1].  

### `init_magics(...)`
Calculates and configures the magic structures and attack tables for every square on the board[cite: 1]:  
* **Mask Calculation**: Determines slider attack rays on an empty board and trims outer edge squares, since board edges do not alter slider reachability[cite: 1].  
* **Carry-Rippler Traversal**: Iterates through all possible subsets of blockers using the expression `b = (b - mask) & mask`[cite: 1].  
* **PEXT Mode**: Directly populates table entries at index `pext64(b, m.mask)` with the simulated ray attack[cite: 1].  
* **Fallback Mode**: Tests candidate magic numbers until finding one that uniquely maps each blocker configuration without index collisions[cite: 1].  

### `Bitboards::init()`
Initializes and fills all static tables when the engine starts up[cite: 1]:  
* Computes `SquareDistance[s1][s2]` via rank and file coordinate differences[cite: 1].  
* Populates `RookTable`, `BishopTable`, and `Magics` through `init_magics`[cite: 1].  
* Calculates `LineBB`, `BetweenBB`, and `RayPassBB` for all collinear square pairings[cite: 1].  

### `Bitboards::pretty(Bitboard b)`
Renders a given 64-bit bitboard into an ASCII-formatted 8x8 chessboard representation (`X` for set bits, empty spaces for unset bits) complete with rank and file coordinates for debugging purposes[cite: 1].  

---

## 7. Fast Attack Query API
* **`PseudoAttacks` Table**: A compile-time generated array providing empty-board attack masks for Pawns, Knights, Kings, and sliding pieces across all 64 squares[cite: 2].  
* **`attacks_bb<PieceType>(Square s, ...)` / `attacks_bb(Piece pc, Square s, ...)`**:
  * **Pawns, Knights, Kings**: Retrieved directly from the compile-time `PseudoAttacks` table without consulting board occupancy[cite: 2].  
  * **Rooks & Bishops**: Retrieved via `Magics[s][pt - BISHOP].attacks_bb(occupied)` to calculate dynamic attacks factoring in blocking pieces[cite: 2].  
  * **Queens**: Computed by combining sliding rook and bishop lookups: `attacks_bb<BISHOP>(s, occ) | attacks_bb<ROOK>(s, occ)`[cite: 2].