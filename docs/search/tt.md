# Transposition Table System (`tt.h` & `tt.cpp`)

This document details the transposition table implementation defined in `tt.h` and implemented in `tt.cpp`. It is written so that anyone can understand how the engine caches previously evaluated positions, avoids redundant search computations, packs memory into CPU cache lines, and handles table replacements.

---

## 1. What is a Transposition Table?

In chess, different sequences of moves often transpose into the exact same board position (for example, `1. d4 Nf6 2. c4` vs `1. c4 Nf6 2. d4`). 

The **Transposition Table (TT)** acts as a high-speed memory cache (hash map)[cite: 11, 12]:
* When the engine searches a position, it stores the calculated score, the best move found, the depth searched, and the evaluation bound.
* If the search encounters that identical position again down another branch, it can look up the result instantly instead of repeating expensive search tree evaluations[cite: 11, 12].

---

## 2. Bound Types (`enum Bound`)

When alpha-beta search analyzes a chess node, it does not always produce an exact score due to pruning cutoffs. The engine tags each cached result with a bound type:

* **`BOUND_NONE` (0):** Uninitialized or invalid score.
* **`BOUND_UPPER` (1) — Fail-Low (All-Node):** Every tested move was worse than the current baseline score (`score <= alpha`). The real evaluation is at most this value.
* **`BOUND_LOWER` (2) — Fail-High (Cut-Node):** A move was found that immediately surpassed the opponent's counter-strategy (`score >= beta`). The real evaluation is at least this value.
* **`BOUND_EXACT` (3) — PV-Node:** The search found a precise score lying inside the search window (`alpha < score < beta`).

---

## 3. Data Layout & Packing

Cache layout directly dictates lookup speed[cite: 11, 12]. The TT structures are packed to prevent CPU cache misses[cite: 11, 12].

### `struct TTEntry` (10 Bytes)
Every table entry is packed down to exactly **10 bytes** using compact integer types:

```text
+-------------------+--------------------+--------------------+
| Field             | Type & Width       | Description        |
+-------------------+--------------------+--------------------+
| key16             | uint16_t (2 bytes) | Upper 16 bits of   |
|                   |                    | the Zobrist key    |
+-------------------+--------------------+--------------------+
| move16            | uint16_t (2 bytes) | Best move found    |
+-------------------+--------------------+--------------------+
| value16           | int16_t  (2 bytes) | Search evaluation  |
+-------------------+--------------------+--------------------+
| eval16            | int16_t  (2 bytes) | Static evaluation  |
+-------------------+--------------------+--------------------+
| depth8            | int8_t   (1 byte)  | Search depth reached|
+-------------------+--------------------+--------------------+
| genBound          | uint8_t  (1 byte)  | Generation (6 bits)|
|                   |                    | + Bound (2 bits)   |
+-------------------+--------------------+--------------------+
Total: 10 bytes (Enforced by static_assert)
```

### Member Methods
* **`move()` / `value()` / `eval()` / `depth()`**: Unpacks and casts the stored compact integer fields back into native engine types (`Move`, `Value`, `Depth`)[cite: 12].  
* **`bound()`**: Extracts the lowest 2 bits from `genBound` via the bitmask `genBound & 0x3`[cite: 12].  
* **`generation()`**: Extracts the upper 6 bits from `genBound` via the bitmask `genBound & 0xFC`[cite: 12].  
* **`save(k, v, b, d, m, ev, gen)`**: Updates the entry details with new search evaluation metrics[cite: 12]. It preserves the existing best move if no new valid move is supplied, preventing high-quality moves from being overwritten by shallow search passes on identical positions[cite: 12].  

---

### `struct TTCluster` (32 Bytes)
* **Structure**: Groups `3` individual `TTEntry` instances ($3 \times 10 = 30\text{ bytes}$) alongside `2` bytes of explicit padding, making the cluster structure measure exactly **32 bytes**[cite: 12].  
* **Alignment (`alignas(32)`)**: Exactly two clusters pack into a single 64-byte hardware cache line[cite: 11, 12]. Probing inspects 3 candidate entries in parallel while requiring only a single memory bus transfer[cite: 11, 12].  

---

## 4. `TranspositionTable` Engine

### Memory Allocation & Alignment (`resize`)
* Accepts the target memory allocation in megabytes (`mbSize`)[cite: 11].  
* Computes `clusterCount_` by dividing the requested memory pool by the 32-byte cluster size[cite: 11].  
* Invokes `std::malloc` to allocate the raw backing buffer with a 64-byte overhead allowance, then uses `std::align` to lock the pointer onto an exact **64-byte cache-line boundary**[cite: 11].  
* Emits an error to standard error and halts the program via `std::abort()` if heap allocation fails[cite: 11].  

### Table Indexing (`index`)
* Uses 128-bit integer multiplication on GCC and Clang:  
  `index = (uint128_t(key) * clusterCount_) >> 64`[cite: 12]  
* Converts the 64-bit Zobrist key into a cluster array index using fast multiplication and bit shifting, bypassing the slower hardware modulo (`%`) instruction[cite: 12].  

### Probing & Replacement Policy (`probe`)
When searching for a position matching a 64-bit Zobrist key (`key`)[cite: 11]:  
* **Direct Hit Check**: Examines all 3 entries within the indexed cluster[cite: 11]. If an entry's `key16` matches the top 16 bits of the key and it is marked active (`genBound != 0`), it returns the entry with `found = true`[cite: 11].  
* **Replacement Selection**: If all cluster slots belong to different positions, the engine identifies the least valuable entry to replace using a heuristic score[cite: 11]:  
  $$\text{Score} = (\text{candidate generation matches current} \ ? \ 0 : 256) - \text{depth}$$  
  * Entries from older search generations receive a $+256$ score penalty, making them the first candidates for replacement[cite: 11].  
  * Within the current generation, entries explored at shallower search depths are chosen for overwriting before deeper, more expensive evaluations[cite: 11].  

### Cache Prefetching (`prefetch`)
* **`prefetch(Key key)`** issues processor cache prefetch instructions (`__builtin_prefetch` on GCC/Clang or `_mm_prefetch` on MSVC) to pull the target cluster into the L1 CPU cache ahead of time[cite: 12].  
* Called within `Position::do_move` to overlap memory fetch latency with move execution logic[cite: 12].  

### Generation Aging (`new_search`)
* Increments `generation_` by `4` at each new search root or move iteration[cite: 12].  
* Stepping by 4 updates only the upper 6 bits of the composite `genBound` byte without disturbing the lower 2 bits allocated for `Bound` flags[cite: 12].  

### Hash Utilization (`hashfull`)
* Measures the permille (parts per 1,000) of the transposition table currently occupied by entries belonging to the active generation[cite: 11].  
* Samples 1,000 evenly spaced cluster locations across the table to calculate table fullness without scanning through gigabytes of memory[cite: 11].  
* Provides the metric queried by UCI frontends to display hash saturation inside the graphical interface[cite: 11].