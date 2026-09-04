# Move Generation Engine (`movegen.h` & `movegen.cpp`)

This document provides a unified, plain-English reference for the engine's move generator defined in `movegen.h` and implemented in `movegen.cpp`[cite: 9, 10]. It details the move generation categories, stage-based search integration, pawn mechanics, ray and jump attacks, evasion generation when in check, and stack-allocated move buffers[cite: 9, 10].

---

## 1. High-Level Architecture

In modern chess engines, generating every single legal move upfront is wasteful[cite: 9, 10]. In quiescent search (evaluating tactically volatile terminal nodes) or during alpha-beta move ordering, the engine only cares about specific subsets of moves (such as captures first, or quiet moves only after captures fail to cause a cutoff)[cite: 9, 10].

The engine implements a **staged, template-specialized generator**[cite: 9, 10]:
* Uses compile-time templates (`Color Us`, `GenType Type`) to eliminate branch mispredictions and dynamic dispatch inside tight generation loops.
* Uses 64-bit bitboard operations to slide and jump pieces across the board in bulk rather than testing moves square-by-square.
* Operates on contiguous memory arrays via pointers (`Move*` or `ExtMove*`), enabling zero heap allocations[cite: 9, 10].

---

## 2. Generation Modes (`enum GenType`)

The generation routine can be instructed to generate only the exact classes of moves required for the current search phase[cite: 9, 10]:

| Mode | What It Generates | Search Use Case |
| :--- | :--- | :--- |
| `CAPTURES` | Piece captures, en passant captures, and pawn Queen-promotions. | Quiescence search and high-priority move ordering[cite: 9, 10]. |
| `QUIETS` | Non-capturing moves, non-queen promotions (underpromotions), and castling moves. | Searched only after tactical captures fail to cause a cutoff[cite: 9, 10]. |
| `EVASIONS` | Moves that escape, block, or capture an active check. | Triggered exclusively when `pos.checkers()` is non-zero. |
| `NON_EVASIONS` | All legal and pseudo-legal moves on the board when the king is **not** in check. | Main alpha-beta search nodes[cite: 9, 10]. |
| `LEGAL` | Strictly legal moves guaranteed not to leave the moving side's king in check. | Perft testing, root position moves, and GUI command interfaces[cite: 9, 10]. |

---

## 3. Data Structures: `ExtMove` and `MoveList`

### `struct ExtMove` (Extended Move)
Inherits directly from `Move` and appends a sorting heuristic score:
* **`value` (`int`)**: Holds an ordering score (such as MVV-LVA — *Most Valuable Victim, Least Valuable Attacker*, history heuristic score, or killer move bonus)[cite: 10].
* **`operator<`**: Enables standard library sorting algorithms (`std::sort`) to order moves from highest expected score to lowest[cite: 10].

### `class MoveList<GenType>`
A stack-allocated container designed for fast and safe move iteration[cite: 10]:
* **Zero Allocation:** Wraps a fixed-size `std::array<Move, MAX_MOVES>` directly on the stack[cite: 10].
* **RAII Construction:** Instantiating `MoveList<LEGAL> list(pos);` immediately populates the buffer up to `last_`[cite: 10].
* **STL Iterator Support:** Exposes `begin()` and `end()` raw pointers, enabling range-based `for` loop traversal[cite: 10]:
  ```cpp
  for (const Move m : MoveList<CAPTURES>(pos)) {
      // Analyze tactical moves directly...
  }
  ```

* **Convenience Helpers:** Exposes `.size()`, `.empty()`, `operator[]`, and `.contains(Move)` for simple inspection and container access[cite: 10].

---

## 4. Pawn Move Generation (`generate_pawn_moves`)

Pawns are the most intricate pieces to generate because movement direction is asymmetric, capturing paths differ from standard forward pushes, double pushes depend on two contiguous empty squares, and special rules govern promotions and en passant captures[cite: 9].

### Single & Double Pushes
* **Single Pushes:** Shifts the friendly pawn bitboard forward by `Up` (+8 for White, -8 for Black) and isolates vacant destinations by intersecting with `emptySquare`[cite: 9].
* **Double Pushes:** Takes the subset of single pushes that reached the relative 3rd rank, advances them another rank forward, and ensures that destination square is also vacant[cite: 9].
* **Evasion Masking:** When operating under `EVASIONS` mode, push bitboards are intersected with the `target` mask to retain only advances that resolve the check[cite: 9].

### Promotions
* **Trigger:** Pawns placed on the 7th relative rank that advance to the 8th relative rank invoke the promotion generator[cite: 9].
* **Tactical Captures Mode:** Under `CAPTURES` mode, only Queen promotions are produced to prioritize immediate material value[cite: 9].
* **Non-Capture Modes:** Under non-capturing modes, underpromotions to Knight, Bishop, and Rook are also emitted[cite: 9].

### Pawn Captures
* **Diagonal Shifts:** Shifts the pawn bitboards diagonally forward-left (`UpLeft`) and forward-right (`UpRight`)[cite: 9].
* **Target Collision:** Intersects the shifted squares with enemy pieces via `pos.pieces(~Us) & target`[cite: 9].
* **Back-Rank Promotions:** If a diagonal capture reaches the back rank (`Rank8`), it is stored as a promotional capture[cite: 9].

### En Passant
* **Square Availability:** Verifies if a valid en passant target exists (`pos.ep_square() != SQ_NONE`)[cite: 9].
* **Eligibility Query:** Masks friendly pawns against reverse pawn-attack patterns stemming from the en passant square[cite: 9].
* **Move Creation:** Appends an `EN_PASSANT` move flag for each eligible capturing pawn[cite: 9].

---

## 5. Piece Move Generation (`generate_piece_moves`)

Non-pawn moves leverage precomputed bitboard geometry and magic tables for fast generation[cite: 9]:

* **Knights & Kings:** Query compile-time attack masks (`attacks_bb<KNIGHT>` and `attacks_bb<KING>`) and intersect them with valid `target` squares[cite: 9].
* **Bishops, Rooks & Queens:** Query the Magic Bitboard tables with current board occupancy to determine dynamic attack rays, masking the result with `target`[cite: 9].
* **Iteration & Emission:** Iteratively pulls each destination square via `pop_lsb(attacks)` and appends the move directly into the output buffer: `*moveList++ = Move(from, to)`[cite: 9].

---

## 6. Check Evasion Mechanics (`generate_evasions`)

When the king is in check, generation routes through special escape logic[cite: 9]:

### King Escapes First
* Generates all standard king moves, excluding squares occupied by friendly pieces[cite: 9].
* Validates every landing square with `pos.attackers_to_exist(...)` to guarantee the king does not step onto an attacked square[cite: 9].

### Double Check Handling
* Evaluates whether multiple pieces deliver check using `more_than_one(pos.checkers())`[cite: 9].
* If true, **only king evasion moves are valid**; the generator returns immediately without processing moves for any other piece[cite: 9].

### Single Check Resolution
When only a single enemy checker exists, non-king pieces can resolve the check in two ways[cite: 9]:
1. **Capture the Checker:** Target the attacking piece directly (`checkerSq`)[cite: 9].
2. **Interpose along the Ray:** Block the line of sight between the king and sliding attacker (`between_bb(ksq, checkerSq)`)[cite: 9].
* Combines both tactical options into a single unified `target` mask (`between_bb | checkerSq`) and restricts all standard piece move generation to those squares[cite: 9].

---

## 7. Dispatching & Full Legality Verification

The top-level dispatch interface `generate_dispatch` routes calls based on `GenType`[cite: 9]:

* **`EVASIONS`:** Directly routes to `generate_evasions`[cite: 9].
* **`CAPTURES`:** Sets the destination target mask to enemy pieces: `pos.pieces(~us)`[cite: 9].
* **`QUIETS`:** Sets the destination target mask to empty squares: `~pos.pieces()`[cite: 9].
* **`NON_EVASIONS`:** Sets the destination target mask to any square not occupied by a friendly piece: `~pos.pieces(us)`[cite: 9].
* **`LEGAL`:**
  * Generates pseudo-legal evasions if currently in check, or non-evasions if not in check[cite: 9].
  * Validates every candidate move through `pos.legal(*cur)`[cite: 9].
  * Filters out invalid moves, such as moving pinned pieces off their defensive lines of sight[cite: 9].