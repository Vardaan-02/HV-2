# High-Performance Stack Containers & Multidimensional Arrays

This document details the utilities defined in `stack_vector.h`, specifically `StackVector` and `MultiArray`[cite: 3]. These data structures provide high-speed, zero-heap memory management tailored for performance-critical chess engine routines such as move generation and history heuristic tables[cite: 3].

---

## 1. Overview & Purpose

In a chess engine, standard dynamic containers like `std::vector` introduce heap allocations and pointer indirections, which add latency during recursive search loops[cite: 3]. 

The utilities in `Engine::Utils` solve this by:
* Allocating storage strictly on the CPU thread's stack or directly inside surrounding objects, eliminating dynamic memory allocation overhead[cite: 3].
* Providing contiguous memory layouts for predictable cache access[cite: 3].
* Supporting compile-time sizing and bounds checking via assertions[cite: 3].

---

## 2. `StackVector<T, Capacity>`

A fixed-capacity, cache-friendly array that behaves like a standard dynamically resizable vector without ever touching the heap[cite: 3].

### Template Parameters
* **`typename T`**: The type of elements being stored (e.g., `Move` structures)[cite: 3].
* **`std::size_t Capacity`**: The maximum number of elements the container can hold, determined at compile time[cite: 3].

### Member Type Definitions
Provides standard container type aliases (`value_type`, `size_type`, `difference_type`, `reference`, `const_reference`, `pointer`, `const_pointer`, `iterator`, `const_iterator`) so the container works seamlessly with C++ algorithms and range-based `for` loops[cite: 3].

### Modifiers

* **`push_back(const T& value)` / `push_back(T&& value)`**  
  * Appends an element to the end of the collection[cite: 3].
  * Copies or moves the value into place and increments the internal element count[cite: 3].
  * Asserts that `size_ < Capacity` to guarantee it does not write past allocated memory[cite: 3].

* **`emplace_back(Args&&... args)`**  
  * Constructs an element directly at the tail index using forwarded arguments[cite: 3].
  * Returns a reference to the newly constructed object[cite: 3].

* **`pop_back()`**  
  * Decrements the active element count by one[cite: 3].
  * Asserts that the vector is not empty (`size_ > 0`)[cite: 3].

* **`clear()`**  
  * Resets the active size counter to `0` in $O(1)$ time without reallocating or deleting the underlying memory buffer[cite: 3].

* **`make_space(size_type count)`**  
  * Advances the internal size counter by `count` and returns a raw pointer to the start of that uninitialized region[cite: 3].
  * Allows external routines (such as low-level move generation buffers) to write multiple elements directly into the buffer in bulk without repetitive `push_back` overhead[cite: 3].

### Element Access

* **`operator[](size_type index)`**  
  * Retrieves a mutable or immutable reference to the item at `index`[cite: 3].
  * Asserts that `index < size_` to catch out-of-bounds reads or writes during debug runs[cite: 3].

* **`front()` / `back()`**  
  * Returns references to the first element (`data_[0]`) or the last active element (`data_[size_ - 1]`)[cite: 3].
  * Asserts that `size_ > 0`[cite: 3].

* **`data()`**  
  * Returns a raw pointer to the underlying contiguous C-style array (`data_`)[cite: 3].

### Iterators & Iteration Support

* **`begin()`, `end()`**: Raw pointer iterators pointing to the first item (`data_`) and one past the last valid item (`data_ + size_`)[cite: 3].
* **`cbegin()`, `cend()`**: Const iterator equivalents ensuring read-only traversal[cite: 3].
* Enables standard range syntax: `for (const auto& move : move_list)`[cite: 3].

### Capacity & Size Queries

* **`empty()`**: Returns `true` if `size_ == 0`[cite: 3].
* **`size()`**: Returns the current count of active elements as `std::size_t`[cite: 3].
* **`ssize()`**: Returns the count cast to a signed `int`, eliminating signed/unsigned comparison warnings when interacting with loop counters[cite: 3].
* **`capacity()`**: Static compile-time query returning the maximum capacity `Capacity`[cite: 3].

---

## 3. `Detail::MultiArrayHelper` (Internal)

A template metaprogramming helper that recursively transforms a sequence of dimension sizes into nested `std::array` types[cite: 3].

* **Recursive Case (`MultiArrayHelper<T, Dim, Dims...>`):** Nests a `std::array` of child array types[cite: 3].
* **Base Case (`MultiArrayHelper<T, Dim>`):** Resolves to a single-dimensional `std::array<T, Dim>`[cite: 3].

---

## 4. `MultiArray<T, Dim, Dims...>`

A type-safe, compile-time multidimensional array wrapper that guarantees flat, contiguous layout in memory while allowing standard bracket indexing syntax (e.g., `table[color][piece][square]`)[cite: 3].

### Key Use Cases
Heuristic scoring tables, such as:
* **History Heuristics**: Tracking move effectiveness by `[Color][FromSquare][ToSquare]`.
* **Countermove Tables**: Looking up responses indexed by `[PreviousPiece][PreviousToSquare]`.

### Methods & Features

* **`operator[](std::size_t index)`**  
  * Provides access to the outermost dimension slice with an bounds check assertion (`assert(index < Dim)`)[cite: 3].
  * Supports chaining brackets for any number of dimensions (`arr[a][b][c]`)[cite: 3].

* **`fill(const U& val)`**  
  * Resets all values across all dimensions recursively using `fill_recursive`[cite: 3].
  * Automatically traverses all inner dimensions down to the leaf elements and sets each to `static_cast<T>(val)`[cite: 3].

* **`size()`**  
  * Returns the size of the outermost dimension (`Dim`)[cite: 3].