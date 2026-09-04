# General Engine Utilities & System Helpers

This document details the cross-platform system helpers, timing mechanisms, string tokenizers, fast hashers, and thread-synchronized I/O routines declared in `misc.h` and implemented in `misc.cpp`. It is written so that anyone can understand what each function does and why it is needed.

---

## 1. Time Management & Wall Clock

During a chess game, the search algorithm must strictly respect time controls (e.g., remaining clock time, increment per move).

* **`TimePoint` (`int64_t`)**: A signed 64-bit integer tracking time in milliseconds.
* **`now()`**  
  * **What it does:** Reads the current time using `std::chrono::steady_clock` and returns the number of milliseconds elapsed since the system epoch.
  * **Why it matters:** `steady_clock` is monotonic, meaning it will never jump backward if the operating system adjusts its wall clock. This guarantees that time-checking loops during search calculations do not produce timing errors.

---

## 2. Fast String Utilities

UCI (Universal Chess Interface) communication requires parsing incoming commands (such as `position startpos moves e2e4`, `go wtime ...`, or FEN strings).

* **`split(std::string_view s, std::string_view delimiter)`**  
  * **What it does:** Breaks an input string into segments wherever the delimiter appears, returning a list of `std::string_view` tokens[cite: 4, 5].
  * **Why it matters:** Unlike standard string splitting which copies every substring into new memory, `std::string_view` merely points to slices of the original text, avoiding dynamic heap allocations while parsing commands[cite: 4, 5].

* **`trim(std::string& s)`**  
  * **What it does:** Strips leading and trailing whitespace characters (spaces, tabs, newlines) in-place[cite: 4, 5].
  * **How it works:** Employs standard algorithms (`std::find_if`) to locate the first and last non-whitespace characters and erases the excess ends.

* **`is_whitespace(std::string_view s)`**  
  * **What it does:** Returns `true` if the entire string consists solely of whitespace characters or is empty[cite: 4, 5].

---

## 3. File System & Directory Paths

Chess engines frequently need to load external opening books, NNUE neural network weight files, or configuration assets from disk[cite: 4, 5].

* **`read_file_to_string(const std::string& path)`**  
  * **What it does:** Reads an entire file from disk into an in-memory `std::string` in a single read operation[cite: 4, 5].
  * **Return Value:** Wrapped in `std::optional`[cite: 4, 5]. Returns `std::nullopt` if the file does not exist, cannot be opened, or has an invalid size.
  * **Why it matters:** Useful for loading raw binary NNUE network files directly into memory buffers[cite: 4, 5].

* **`get_working_directory()`**  
  * **What it does:** Retrieves the current working directory where the process was invoked[cite: 4, 5].
  * **Cross-Platform Handling:** Automatically routes to `_getcwd` on Windows systems and `getcwd` on Linux/macOS.

* **`get_binary_directory(std::string argv0)`**  
  * **What it does:** Determines the absolute directory where the engine executable binary itself resides on the disk[cite: 4, 5].
  * **Why it matters:** Ensures the engine can locate its companion data files (e.g., neural net files stored next to the binary) even if the user or a chess GUI launches the engine from a different folder[cite: 4, 5].

---

## 4. Thread-Safe Console Output (`sync_cout` / `sync_endl`)

When searching chess positions with multiple parallel threads, multiple worker threads writing to standard console output (`std::cout`) at the same time can interleave their text, resulting in scrambled, unreadable log messages[cite: 4, 5].

* **`SyncStreamState` (`IO_LOCK`, `IO_UNLOCK`)**: An enum used as a stream marker to engage or disengage output locking[cite: 4, 5].
* **Stream Insertion Operator `<<` Overload:**
  * Intercepts `IO_LOCK` to acquire a global mutex (`cout_mutex`), blocking other threads from printing[cite: 4].
  * Intercepts `IO_UNLOCK` to release `cout_mutex` once printing is complete[cite: 4].
* **Macros:**
  * **`sync_cout`**: Equivalent to writing `std::cout << IO_LOCK`[cite: 5].
  * **`sync_endl`**: Equivalent to writing `std::endl << IO_UNLOCK`[cite: 5].
* **Usage Pattern:**

```cpp
  sync_cout << "info depth 12 score cp 45 time 350 nodes 120500" << sync_endl;
```

This ensures the entire output line is written to the terminal as an uninterrupted atomic block without thread collisions[cite: 4, 5].

---

## 5. Fast Hash Utilities

* **`hash_bytes(const char* data, std::size_t size)`**  
  * **What it does:** Computes a 64-bit hash over a raw sequence of bytes using the **FNV-1a (Fowler–Noll–Vo)** hashing algorithm[cite: 5].  
  * **Why it matters:** FNV-1a is an extremely lightweight, non-cryptographic hash with minimal CPU overhead, making it ideal for fast lookups on binary buffers, internal states, or raw memory chunks[cite: 5].  

* **`hash_string(std::string_view sv)`**  
  * **What it does:** A lightweight inline convenience function that forwards the string's underlying character buffer and length to `hash_bytes`[cite: 5].  

* **`hash_combine(std::size_t& seed, const T& v)`**  
  * **What it does:** Integrates a new value `v` into an existing accumulator `seed` using the golden ratio bitwise mixing constant ($0\text{x}9\text{E}3779\text{B}97\text{F}4\text{A}7\text{C}15$) along with bit-shifts[cite: 5].  
  * **Why it matters:** Allows multi-field structures (such as combining a `Move` with an evaluation score or search depth) to produce a composite hash value with minimal hash collision rates[cite: 5].  

---

## 6. Version & Compiler Diagnostics

* **`engine_version_info()`**  
  * **What it does:** Returns the engine's release identity banner string (`"V-Chess 1.0 (dev)"`)[cite: 4, 5].  

* **`compiler_info()`**  
  * **What it does:** Inspects preprocessor flags to generate a diagnostic string of the compilation environment[cite: 4, 5]:  
    * **Compiler Version:** Detects and formats the toolchain identity and version numbers for Clang, GCC, or MSVC[cite: 4].  
    * **Operating System:** Identifies whether the binary target is Linux, Windows (64-bit), or macOS[cite: 4].  
    * **SIMD & Instruction Set Flags:** Verifies hardware vector extensions compiled into the binary, including `AVX-512`, `AVX2`, `BMI2(PEXT)`, or `ARM NEON`[cite: 4].