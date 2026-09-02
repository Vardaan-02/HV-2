# Low-Level Optimization & Bit-Manipulation Reference

This document explains the core directives, compiler attributes, and low-level performance functions used in this module.

---

### File Structure & Header Directives

* **`#pragma once` (Line 1)**  
  * **What it does:** Instructs the compiler's preprocessor to include this file only once per compilation unit.
  * **Why it matters:** Prevents duplicate definition errors caused by circular or redundant imports.

* **Core Headers (Lines 3–19)**  
  * **What it does:** Imports hardware-accelerated bit-manipulation utilities and specialized x86/x86-64 CPU instructions.
  * **Why it matters:** Gives the program direct access to assembly-level instructions without needing to write inline assembly.

---

### Compiler Attributes

* **`[[nodiscard]]`**  
  * **What it does:** Emits a compiler warning if the caller invokes the function but ignores or discards its return value.
  * **Why it matters:** Ensures critical computation results (such as calculated masks or moves) are not accidentally dropped, preventing subtle bugs.

---

### Functions

#### `pext64` (Parallel Bit Extract)

Extracts specific bits from a 64-bit value using a bitmask and packs them into a contiguous sequence.

* **Real-World Analogy:**  
  Imagine a line of numbered boxes. You give a helper a checklist (the mask) with checkmarks on specific boxes. The helper takes only the checked items and slides them neatly to the front of a new tray, leaving no empty spaces between them.

* **Step-by-Step Operation:**
  1. **Scans the mask:** Inspects the bitmask from right to left, starting from the Least Significant Bit (LSB) toward the Most Significant Bit (MSB).
  2. **Extracts matching bits:** Whenever a `1` is encountered in the mask, it extracts the bit at that exact position from the source value (`val`).
  3. **Packs the output:** Slides all extracted bits into the lowest bits of the result, padding the remaining upper bits with zeros.

---

#### `prefetch` (Hardware Cache Prefetch)

Loads memory into high-speed CPU caches before the program actually needs to read or write it.

* **Real-World Analogy:**  
  A chef asking an assistant to bring ingredients from the cold storage room to the kitchen counter 5 minutes before cooking, eliminating wait time.

* **Key Benefits:**
  * **Reduces Latency:** Pulls memory from high-latency system RAM into low-latency L1/L2/L3 cache lines ahead of time.
  * **Hides Memory Stalls:** Overlaps memory fetch delays with ongoing arithmetic or logic operations.