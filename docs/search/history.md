# Search Move Ordering Heuristics Reference

This document explains the search heuristic tables, scoring updates, and static evaluation values defined in the search ordering header. It details how the engine learns which moves are most effective during tree search to achieve alpha-beta cutoffs as early as possible.

---

## 1. Move Ordering Overview

In an alpha-beta search tree, searching the best move first allows the engine to prune away most alternative branches, dramatically reducing the total nodes evaluated.

Because the engine cannot know the best move in advance, it uses **heuristic memory tables**:
* **History Heuristic:** Tracks which moves frequently cause beta cutoffs across the entire game tree.
* **Countermove Heuristic:** Tracks specific move refutations that effectively punish the opponent's previous move.
* **Killer Move Heuristic:** Remembers quiet moves that caused cutoffs at the exact same search depth.

---

## 2. History Scoring & Dynamic Damping

* **`HISTORY_MAX` (`16384`)**  
  The maximum score ceiling for history values. Keeping the limit at $16384$ ensures values fit comfortably inside signed 16-bit integers (`int16_t`) without overflowing.

* **`update_history_score(int16_t& val, int bonus)`**  
  * **What it does:** Updates a move's historical score using a **gravity-based damping formula**:
    $$\text{val} \leftarrow \text{val} + \text{bonus} - \frac{\text{val} \times |\text{bonus}|}{\text{HISTORY_MAX}}$$
  * **Why it matters:** As scores grow larger, the negative damping term increases proportionally. This prevents the scores from saturating at maximum values, naturally decay older move scores, and keeps recent search trends relevant.

---

## 3. Butterfly History Table (`ButterflyHistory`)

Tracks overall move effectiveness across the board, indexed strictly by `[Color][FromSquare][ToSquare]`.

* **Storage:** A flat array of size $2 \times 64 \times 64 = 8{,}192$ entries (`int16_t`), requiring only 16 KB of RAM.
* **Indexing (`index`):** Maps the 3D coordinate `(color, from, to)` into a contiguous 1D array index:
  $$\text{Index} = (c \times 4096) + (\text{from} \times 64) + \text{to}$$
* **Methods:**
  * **`clear()`:** Wipes the history table to zeros between moves or searches.
  * **`get(Color c, Square from, Square to)`:** Returns the accumulated history score for a candidate move.
  * **`update(Color c, Square from, Square to, int bonus)`:** Applies a bonus (for moves causing a cutoff) or penalty (for moves searched before the cutoff) using `update_history_score`.

---

## 4. Countermove Table (`CounterMoveTable`)

Refutation table that answers the question: *"When the opponent moves piece $P$ to square $S$, what move typically refutes it?"*

* **Storage:** Indexed by `[PreviousPiece][PreviousToSquare]` ($16 \times 64 = 1{,}024$ entries) holding `Move` objects.
* **Indexing (`index`):**
  $$\text{Index} = (\text{piece} \times 64) + \text{square}$$
* **Methods:**
  * **`clear()`:** Resets all slots to `Move::none()`.
  * **`get(Piece prevPiece, Square prevTo)`:** Returns the stored refutation move if valid coordinates are supplied; otherwise returns `Move::none()`.
  * **`update(Piece prevPiece, Square prevTo, Move refutation)`:** Records the refutation move that successfully caused a beta cutoff against the opponent's previous move.

---

## 5. Killer Moves Table (`KillerTable`)

Stores two high-priority non-capture ("quiet") moves per ply that caused a beta cutoff at that specific depth in sibling branches.

* **Capacity:** Holds moves for up to 128 plies (`MAX_PLY = 128`), with two slots per ply:
  * **`primary`**: The most recently successful killer move.
  * **`secondary`**: The previous killer move bumped down from the primary slot.
* **Methods:**
  * **`clear()`:** Clears both primary and secondary arrays to `Move::none()`.
  * **`primary(int ply)` / `secondary(int ply)`:** Retrieves the stored killer moves for the current search depth.
  * **`update(int ply, Move m)`:**
    * If `m` is already the primary killer, no action is taken (avoids duplicate entries).
    * Otherwise, shifts the current primary move into the secondary slot and stores `m` as the new primary killer.

---

## 6. Static Exchange Evaluation Table (`SEE_VALUES`)

A fast piece valuation array used by Static Exchange Evaluation (SEE) routines to judge whether a tactical capture sequence is profitable before exploring it deeper:

| Piece Type | SEE Value | Traditional Pawn Unit |
| :--- | :--- | :--- |
| `NO_PIECE_TYPE` | 0 | 0.0 |
| `PAWN` | 100 | 1.0 |
| `KNIGHT` | 320 | 3.2 |
| `BISHOP` | 330 | 3.3 |
| `ROOK` | 500 | 5.0 |
| `QUEEN` | 900 | 9.0 |
| `KING` | 20000 | $\infty$ (Cannot be captured) |