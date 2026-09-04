# Board Representation & Game State Management (`Position`)

This document provides a unified, plain-English reference for both `position.h` and `position.cpp`[cite: 7, 8]. It explains the board representation, Zobrist hashing, Cuckoo repetition detection, FEN parsing, check and pin calculation, move execution (`do_move`/`undo_move`), and Static Exchange Evaluation (SEE)[cite: 7, 8].

---

## 1. Architectural Architecture & Dual Representation

To achieve high speeds during tree search, the engine stores the board in two parallel formats[cite: 8]:
1. **Mailbox Board (`board[64]`):** A direct array mapping every square (`0` to `63`) to the piece standing on it (`board[s]`)[cite: 8]. Provides $O(1)$ answers to *"What is standing on square E4?"*[cite: 8]
2. **Bitboard Arrays (`byTypeBB` and `byColorBB`):** 64-bit masks tracking locations by piece type and color[cite: 8]. Allows parallel bitwise operations for questions like *"Give me all white pawns"* or *"Find all pieces blocking this ray"* in a single clock cycle[cite: 8].

Rather than copying the entire board state across millions of search nodes, the engine updates the board **in-place** using `do_move()` and rolls back changes using `undo_move()`[cite: 7, 8].

---

## 2. Incremental State Tracking (`struct StateInfo`)

Certain chess rules depend on move history that cannot be deduced from a static board diagram (such as whether castling rights were lost three moves ago, or whether an en passant capture is currently available)[cite: 8]. 

`StateInfo` objects form a linked list via the `previous` pointer across the search call stack[cite: 7, 8]:

### Historical Keys & Balances
* **`key`**: The 64-bit Zobrist hash of the current position, used to index the Transposition Table[cite: 7, 8].
* **`pawnKey`**: A specialized hash tracking pawn structure alone, allowing the engine to reuse pawn structure evaluation across different positions[cite: 7, 8].
* **`materialKey`**: A hash tracking the remaining piece count inventory regardless of their positions[cite: 7, 8].
* **`minorPieceKey`**: A hash tracking Knights and Bishops[cite: 7, 8].
* **`nonPawnKey[COLOR_NB]`**: Independent hashes tracking major and minor pieces for each side[cite: 7, 8].
* **`nonPawnMaterial[COLOR_NB]`**: Running material tally (excluding pawns) for White and Black, used to detect endgame phases[cite: 7, 8].
* **`castlingRights`**: Active bit flags for remaining castling options[cite: 7, 8].
* **`rule50`**: Counter for the 50-move draw rule (resets on pawn moves or captures)[cite: 7, 8].
* **`pliesFromNull`**: Moves elapsed since the last null move, preventing illegal recursive null-move pruning[cite: 7, 8].
* **`epSquare`**: Active en passant target square (`SQ_NONE` if none)[cite: 7, 8].

### Dynamic Tree & Tactical Diagnostics
* **`checkersBB`**: A bitboard of all enemy pieces currently giving check to the friendly king[cite: 7, 8].
* **`previous`**: Pointer to the parent move's `StateInfo`, restored on `undo_move`[cite: 7, 8].
* **`blockersForKing[COLOR_NB]` / `pinners[COLOR_NB]`**: Masks of pinned pieces defending the king and the enemy sliding pieces pinning them[cite: 7, 8].
* **`checkSquares[PIECE_TYPE_NB]`**: Precalculated masks of squares where a given piece type would deliver check if moved there[cite: 7, 8].
* **`capturedPiece`**: The exact piece taken during the move, allowing `undo_move` to restore it[cite: 7, 8].
* **`repetition`**: Tracks whether the position has occurred previously along the search path[cite: 7, 8].

---

## 3. Zobrist Hashing & Cuckoo Repetition Table

The engine uses **Zobrist Hashing** to assign a 64-bit integer signature to any chess position[cite: 7].

### Random Keys (`namespace Zobrist`)
Initialized inside `Position::init()` with a deterministic SplitMix pseudo-random generator (`SimplePRNG`)[cite: 7]:
* `psq[PIECE_NB][SQUARE_NB]`: Random 64-bit numbers for every piece on every square (pawns on ranks 1 and 8 are set to `0`)[cite: 7].
* `enpassant[FILE_NB]`: Keys for the active en passant column[cite: 7].
* `castling[CASTLING_RIGHT_NB]`: Keys for every permutation of castling rights[cite: 7].
* `side`: Toggled when it is Black's turn to move[cite: 7].
* `noPawns`: Base initialization constant for `pawnKey`[cite: 7].

### Cuckoo Hashing for Fast Repetition Detection
* **Tables:** `cuckoo[8192]` and `cuckooMove[8192]`[cite: 7].
* **How it works:** During startup, all reversible non-pawn moves (moves that can be undone immediately) are stored using two hash indexes (`H1` and `H2`)[cite: 7]. 
* **`upcoming_repetition(ply)`**: Uses these cuckoo tables to look up to 12+ ply back along the game path in $O(1)$ steps to detect upcoming three-fold repetitions before executing speculative moves in the search[cite: 7, 8].

---

## 4. Setup, FEN Parsing & Export

* **`Position::init()`**  
  * Seeds the PRNG, generates all Zobrist bitstrings, and pre-populates the Cuckoo tables[cite: 7, 8].

* **`Position::set(fenStr, isChess960, StateInfo* si)`**  
  * Wipes the existing position and initializes `st = si`[cite: 7, 8].
  * **Step 1 (Pieces):** Reads FEN rows top-to-bottom (rank 8 down to 1), populating the mailbox and bitboard structures[cite: 7].
  * **Step 2 (Turn):** Reads active color (`w` or `b`)[cite: 7].
  * **Step 3 (Castling):** Parses `K`, `Q`, `k`, `q` or Chess960 column letters (`A`–`H`), setting castling paths and rook origin squares via `set_castling_right`[cite: 7].
  * **Step 4 (En Passant):** Validates the en passant square by verifying that an enemy pawn actually made a double step and a friendly pawn can legally capture it[cite: 7].
  * **Steps 5 & 6 (Clocks):** Parses the 50-move clock and fullmove counters, sets `chess960` mode, and executes `set_state()`[cite: 7].

* **`Position::fen()`**  
  * Re-encodes the active board, active color, castling rights (with Chess960 column letter overrides if enabled), en passant target, and move clocks back into a standard FEN string[cite: 7, 8].

* **`operator<<(std::ostream&, const Position&)`**  
  * Prints an ASCII chessboard representation with rank/file coordinates, the current FEN string, and the active 64-bit Zobrist key in hexadecimal[cite: 7, 8].

* **`flip()`**  
  * Inverts the board perspective (swapping colors, mirroring ranks, and flipping case) for symmetry testing and debugging[cite: 7, 8].

---

## 5. Board Occupancy & Basic Accessors

* **`pieces()`**: Bitboard of all pieces on the board (`byTypeBB[ALL_PIECES]`)[cite: 8].
* **`pieces(Color c)`**: Bitboard of all pieces belonging to color `c`[cite: 8].
* **`pieces(PieceTypes... pts)`**: Variadic fold expression combining multiple piece type bitboards (`(byTypeBB[pts] | ...)`)[cite: 8].
* **`pieces(Color c, PieceTypes... pts)`**: Mask of specified piece types belonging exclusively to color `c`[cite: 8].
* **`piece_on(Square s)` / `empty(Square s)`**: Checks the mailbox array to return the piece on square `s` or verify if it is vacant[cite: 8].
* **`count<PieceType>(Color c)` / `count<PieceType>()`**: Returns piece quantities for one player or the combined total across both sides[cite: 8].
* **`square<PieceType>(Color c)`**: Returns the square index of a single piece (asserts that exactly one copy exists, such as the King)[cite: 8].
* **`ep_square()`**: Returns the current en passant square[cite: 8].

---

## 6. Pins, Checks & Attack Projections

* **`update_slider_blockers(Color c)`**  
  * Locates all enemy Queen, Rook, and Bishop "snipers" aligned with the friendly king[cite: 7, 8].
  * Traces the ray between the king and sniper[cite: 7]. If exactly one piece blocks the ray, that piece is marked in `blockersForKing[c]` (pinned piece)[cite: 7, 8]. If the blocker belongs to side `c`, the attacker square is marked in `pinners[~c]`[cite: 7, 8].

* **`set_check_info()`**  
  * Populates `checkSquares[Pt]` with attack masks from the enemy king's square[cite: 7, 8]. If a friendly piece steps into this mask, it delivers a direct check[cite: 7, 8].

* **`attackers_to(Square s, Bitboard occupied)`**  
  * Computes all pieces (Pawns, Knights, Kings, and sliding rays through `occupied`) that attack square `s`[cite: 7, 8].

* **`attackers_to_exist(Square s, Bitboard occupied, Color c)`**  
  * Early-exit boolean check returning `true` as soon as any attacker of color `c` targeting square `s` is discovered[cite: 7, 8].

* **`attacks_by<PieceType>(Color c)`**  
  * Returns the combined board attack coverage generated by all pieces of type `Pt` for side `c`[cite: 8].

---

## 7. Move Classification & Legality Verification

* **`legal(Move m)`**  
  * Performs strict chess legality checks[cite: 7, 8]:
    * **En Passant:** Ensures capturing en passant does not create a rank-discovered check against the king[cite: 7].
    * **Castling:** Verifies through `attackers_to_exist` that the king does not cross or land on attacked squares[cite: 7].
    * **King Moves:** Confirms the destination square is not under attack[cite: 7].
    * **Pinned Pieces:** Verifies pinned pieces only move along their pinning ray toward or away from the king[cite: 7].

* **`pseudo_legal(Move m)`**  
  * Fast check confirming a move obeys physical piece rules (e.g., knight L-shapes, single/double pawn pushes, non-friendly square landings) and resolves check if the king is currently attacked[cite: 7, 8].

* **`gives_check(Move m)`**  
  * Predicts whether a candidate move delivers direct check, discovered check, or castling check without having to apply the move to the board[cite: 7, 8].

* **`capture(Move m)` / `capture_stage(Move m)`**  
  * Checks if a move captures an opponent's piece or promotes to a Queen (high-priority tactical moves in quiescence search)[cite: 8].

---

## 8. Move Mutation: `do_move` and `undo_move`

### `do_move(Move m, StateInfo& newSt, bool givesCheck)`
1. **Link History:** Copies key state variables into `newSt`, sets `newSt.previous = st`, and advances `st`[cite: 7, 8].
2. **Move Clocks:** Increments `gamePly`, `rule50`, and `pliesFromNull`[cite: 7, 8].
3. **Handle Castling:** If castling, moves both King and Rook using `do_castling<true>` and updates Zobrist signatures[cite: 7].
4. **Handle Captures:** Removes the captured piece (adjusting for en passant if applicable), subtracts its value from `nonPawnMaterial`, clears its hash from `materialKey` and `nonPawnKey`, and resets `rule50 = 0`[cite: 7].
5. **Update Moving Piece:** Moves the piece using XOR operations (`^=`) on bitboards and mailbox slots[cite: 6, 7]. For pawn promotions, substitutes the new piece and adds its material value[cite: 7].
6. **Castling Rights:** Checks if the moving piece or captured piece originated on a castling corner, revoking the corresponding rights via `castlingRightsMask`[cite: 7].
7. **En Passant Availability:** Sets `st->epSquare` if a pawn moved two squares and enemy pawns flank it; otherwise clears it[cite: 7].
8. **Switch Turn:** Toggles `sideToMove = ~sideToMove` and calls `set_check_info()`[cite: 7, 8].
9. **Repetition Tracking:** Traverses back through `st->previous` up to `rule50` plies to check for position recurrence[cite: 7].

### `undo_move(Move m)`
1. Flips `sideToMove` back to the original player[cite: 7].
2. If the move was a promotion, downgrades the promoted piece back to a pawn[cite: 7].
3. Moves the primary piece back from destination to origin[cite: 7].
4. Restores any captured piece recorded in `st->capturedPiece`[cite: 7, 8].
5. If castling, returns both king and rook to their original squares using `do_castling<false>`[cite: 7].
6. Restores the historical state pointer (`st = st->previous`) and decrements `gamePly`[cite: 7, 8].

### `do_null_move(StateInfo& newSt)` / `undo_null_move()`
* Passes the turn without moving a piece[cite: 7, 8].
* Clears any active en passant targets, toggles side to move, resets `pliesFromNull = 0`, and updates check information[cite: 7, 8].

---

## 9. Static Exchange Evaluation (`see_ge`)

`see_ge(Move m, int threshold)` determines whether a sequence of captures on a single square gains material without searching the game tree[cite: 7, 8]:
* Simulates the capture sequence on square `to` in order of least valuable attacker to most valuable attacker (Pawn $\to$ Knight $\to$ Bishop $\to$ Rook $\to$ Queen $\to$ King)[cite: 7].
* Uncovers hidden slider rays behind capturing pieces as pieces are removed from the simulated `occupied` bitboard[cite: 7].
* Accounts for pinned defenders by verifying `pinners` masks[cite: 7].
* Returns `true` if the material balance meets or exceeds `threshold`, allowing the search to prune bad captures early[cite: 7, 8].

---

## 10. Game State Diagnostics

* **`is_draw(ply)`**: Returns `true` if the 50-move rule has triggered (`rule50 > 99` without check) or if repetition is detected[cite: 7, 8].
* **`is_repetition(ply)`**: Verifies if the current state key matched an ancestor state in the active search tree[cite: 7, 8].
* **`has_repeated()`**: Checks whether any position in the history stack has appeared more than once[cite: 7, 8].
* **`non_pawn_material(Color c)` / `non_pawn_material()`**: Returns combined minor/major piece values to determine endgame states or evaluate draw claims[cite: 8].
* **`pos_is_ok()`**: Internal sanity check confirming both kings exist, active side is valid, and the en passant square is positioned on the correct relative 6th rank[cite: 7, 8].