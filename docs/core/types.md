# Chess Engine Core: Types, Constants, and Primitives

This document explains the foundational definitions, chess rules, scoring scales, bitboard helpers, and move encoding used across the chess engine. It is designed so that anyone can understand what each piece of code does and why it exists.

---

## 1. File Directives & Included Libraries

* **`#pragma once`**  
  * **What it does:** Ensures this header file is compiled only once per file build.
  * **Why it matters:** Prevents duplicate definitions when multiple engine modules include it.

* **Standard Library Inclusions (`<cassert>`, `<cstddef>`, `<cstdint>`, `<type_traits>`)**  
  * **What it does:** Imports exact-width integers (like 64-bit and 8-bit unsigned numbers), debugging sanity checks (`assert`), memory sizing types, and compile-time type verification tools.

* **Namespace `Engine::Core`**  
  * **What it does:** Bundles all core chess primitives inside a shared family name to prevent naming conflicts with other libraries.

---

## 2. Fundamental Aliases & Limits

| Identifier | Type | Meaning in Plain English |
| :--- | :--- | :--- |
| `Key` | `uint64_t` | A 64-bit unique digital fingerprint (Zobrist hash) identifying an exact board layout. |
| `Bitboard` | `uint64_t` | A 64-bit number where each of the 64 bits directly represents one of the 64 squares on a chessboard (`1` = piece present, `0` = empty). |
| `MAX_MOVES` | `256` | The safety limit on the maximum legal moves possible in any single chess position. |
| `MAX_PLY` | `246` | The maximum search depth in half-moves (a "ply" is one turn by one player). 246 plies exceeds any standard tactical sequence. |

---

## 3. Core Enums (Game States & Categories)

### Colors (`Color`)
Distinguishes whose turn it is or who owns a piece.
* `WHITE` (0), `BLACK` (1), and `COLOR_NB` (2 = total number of colors).

### Castling Rights (`CastlingRights`)
Tracks whether either king still has the right to castle kingside or queenside.
* Values use **bit flags** (powers of two: `1, 2, 4, 8`) so multiple rights can be combined into one number.
* `WHITE_OO`: White Kingside (short castle).
* `WHITE_OOO`: White Queenside (long castle).
* `BLACK_OO`: Black Kingside.
* `BLACK_OOO`: Black Queenside.
* Combinations like `KING_SIDE`, `QUEEN_SIDE`, `WHITE_CASTLING`, `BLACK_CASTLING`, and `ANY_CASTLING` allow checking multiple rights at once using bitwise math.

### Search Bounds (`Bound`)
Used by the engine's transposition table (memory cache of evaluated positions) to know what kind of score was saved:
* `BOUND_NONE`: No reliable score stored.
* `BOUND_UPPER`: The position is at most this good (failing low).
* `BOUND_LOWER`: The position is at least this good (failing high).
* `BOUND_EXACT`: An exact, fully calculated score.

### Pieces and Piece Types
* **`PieceType`**: The abstract kind of piece regardless of color (`PAWN`, `KNIGHT`, `BISHOP`, `ROOK`, `QUEEN`, `KING`). `PIECE_TYPE_NB` (8) is the total array size allocated.
* **`Piece`**: A concrete colored piece. White pieces occupy IDs `1–6`, and Black pieces are offset by `+8` (`9–14`). This allows extracting the piece type via modulo/masking and color via division/shifting.

### Squares, Files, and Ranks
* **`Square`**: Every square on the board from `SQ_A1` (0) to `SQ_H8` (63). `SQ_NONE` represents an invalid/off-board square.
* **`File`**: Columns `A` through `H` (0 to 7).
* **`Rank`**: Rows `1` through `8` (0 to 7).
* **`Direction`**: Step distances on the 64-square bitboard:
  * `NORTH` (+8), `SOUTH` (-8), `EAST` (+1), `WEST` (-1).
  * Diagonals: `NORTH_EAST` (+9), `SOUTH_EAST` (-7), `SOUTH_WEST` (-9), `NORTH_WEST` (+7).

---

## 4. Evaluation Values & Score Checkers

The engine rates positions using integer points (centipawns, where a pawn is roughly ~200 points in this evaluation tuning):

| Constant | Value | Description |
| :--- | :--- | :--- |
| `VALUE_ZERO` / `VALUE_DRAW` | 0 | Dead equal or forced draw. |
| `VALUE_NONE` | 32002 | Uninitialized / invalid score sentinel. |
| `VALUE_INFINITE` | 32001 | Theoretical maximum cutoff score. |
| `VALUE_MATE` | 32000 | Checkmate score baseline. Faster mates subtract the ply count so the engine prefers mating earlier. |
| `VALUE_TB` | ~31753 | Tablebase win score (known perfect endgame lookup). |
| `PawnValue` | 208 | Base piece value for Pawns. |
| `KnightValue` | 781 | Base piece value for Knights. |
| `BishopValue` | 825 | Base piece value for Bishops. |
| `RookValue` | 1276 | Base piece value for Rooks. |
| `QueenValue` | 2538 | Base piece value for Queens. |

### Evaluation Checker Functions
* **`is_valid(Value)`**: Confirms the score is a real calculated value and not uninitialized (`!= VALUE_NONE`).
* **`is_win(Value)`**: Checks if the score represents a guaranteed tablebase or checkmate win.
* **`is_loss(Value)`**: Checks if the score represents an unavoidable loss.
* **`is_decisive(Value)`**: Checks if the game outcome is definitively resolved (either a win or a loss, not a draw).
* **`mate_in(ply)` / `mated_in(ply)`**: Calculates the exact score for mating or being mated in `ply` half-moves.

---

## 5. NNUE & Neural Network Tracking Structures

These structures track what changed on the board during a move so the neural network evaluation (NNUE) only updates what changed rather than recalculating the entire board from scratch.

### `DirtyPiece`
A record of a piece moved or captured:
* `pc`: The piece being moved.
* `from`, `to`: The starting and ending squares.
* `remove_sq`, `remove_pc`: Any piece removed (e.g., captured piece or en passant target).
* `add_sq`, `add_pc`: Any piece placed on the board (e.g., promoted queen).

### `DirtyThreat`
A packed 32-bit integer tracking changes to piece-on-piece attack threats:
* **Bit packing layout:**
  * Bits `0–7`: Square of the attacking piece (`pc_sq`).
  * Bits `8–15`: Square of the attacked piece (`threatened_sq`).
  * Bits `16–19`: Type of the threatened piece (`threatened_pc`).
  * Bits `20–23`: Type of the attacking piece (`pc`).
  * Bit `31`: Flag indicating whether this threat was added (`1`) or removed (`0`).
* **Why pack into 32 bits?** Allows copying, storing, and updating threat changes with maximum CPU cache efficiency.

---

## 6. Mathematical & Coordinate Helpers

* **`operator++` / `operator--` for Enums**: Allows looping directly through squares, ranks, files, and piece types (e.g., `for (Square s = SQ_A1; s <= SQ_H8; ++s)`).
* **`operator~(Color c)`**: Toggles color (`WHITE` becomes `BLACK`, `BLACK` becomes `WHITE`).
* **`operator~(Piece pc)`**: Flips a piece's color to the opposing side.
* **`flip_rank(Square s)` / `flip_file(Square s)`**: Mirrors a square vertically (rank) or horizontally (file).
* **`make_square(File, Rank)`**: Combines file and rank coordinates into a single 0–63 square index.
* **`make_piece(Color, PieceType)`**: Combines a color and piece type into a unified piece ID.
* **`type_of(Piece)`**: Extracts just the piece type (ignores color).
* **`color_of(Piece)`**: Extracts just the owner color of a piece.
* **`is_ok(Square)`**: Verifies that a square index falls within valid board boundaries (`0` to `63`).
* **`relative_square(Color, Square)` / `relative_rank(Color, Rank)`**: Views the board from Black's perspective if color is `BLACK` (useful for evaluation symmetry, so Black pawns advancing down mirror White pawns advancing up).
* **`pawn_push(Color)`**: Returns `+8` (North) for White and `-8` (South) for Black.
* **`make_key(seed)`**: A 64-bit pseudo-random linear congruential generator step used to hash data into uniform keys.

---

## 7. The `Move` Class

Represents any chess move packed into a compact **16-bit integer (`uint16_t`)**.

### Bit Layout of a Move:
```text
Bits 0–5   (6 bits): Destination square ('to', 0 to 63)
Bits 6–11  (6 bits): Origin square ('from', 0 to 63)
Bits 12–13 (2 bits): Promotion piece type (Knight, Bishop, Rook, Queen)
Bits 14–15 (2 bits): Move type flag (Normal, Promotion, En Passant, Castling)
```

### Methods

* **`Move(from, to)`**  
  Creates a standard move representing a piece stepping from one square to another.

* **`make<MoveType>(from, to, promotion_piece)`**  
  A factory function used to build special moves that require flags, such as castling, en passant captures, or pawn promotions.

* **`from_sq()`**  
  Extracts the origin square (`0` to `63`) where the moving piece started.

* **`to_sq()`**  
  Extracts the destination square (`0` to `63`) where the piece lands.

* **`type_of()`**  
  Decodes the move flags to identify the move category: normal, promotion, en passant, or castling.

* **`promotion_type()`**  
  Identifies which piece a pawn promotes to (Knight, Bishop, Rook, or Queen) if the move is a promotion.

* **`none()`**  
  Returns a blank sentinel move (`value = 0`) representing an uninitialized, invalid, or absent move.

* **`null()`**  
  Returns a special "pass" move (`value = 65`) used in search algorithms for *Null Move Pruning*, where the engine simulates skipping a turn to see if the opponent can pose an immediate threat.

* **`Move::Hash`**  
  A hashing utility structure that allows `Move` objects to be stored and looked up inside fast hash tables like `std::unordered_set` or `std::unordered_map`.

---

## 8. Template Helpers

* **`is_all_same_v<Ts...>`**  
  * **What it does:** A compile-time check that inspects a list of data types and confirms whether every single one is identical.
  * **Why it matters:** Catches type mismatches during compilation with zero performance cost at runtime.