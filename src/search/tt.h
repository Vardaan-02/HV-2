#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/types.h"

namespace Engine::Search {

using Core::Depth;
using Core::Key;
using Core::Move;
using Core::Value;

// Bound types for alpha-beta cutoffs
enum Bound : uint8_t {
    BOUND_NONE  = 0,
    BOUND_UPPER = 1, // Fail-low (All-node): score <= alpha
    BOUND_LOWER = 2, // Fail-high (Cut-node): score >= beta
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER // PV-node: score inside (alpha, beta)
};

// 10-byte packed Transposition Table Entry
struct TTEntry {
    [[nodiscard]] Move  move() const noexcept { return Move(move16); }
    [[nodiscard]] Value value() const noexcept { return static_cast<Value>(value16); }
    [[nodiscard]] Value eval() const noexcept { return static_cast<Value>(eval16); }
    [[nodiscard]] Depth depth() const noexcept { return static_cast<Depth>(depth8); }
    [[nodiscard]] Bound bound() const noexcept { return static_cast<Bound>(genBound & 0x3); }
    [[nodiscard]] uint8_t generation() const noexcept { return static_cast<uint8_t>(genBound & 0xFC); }

    void save(Key k, Value v, Bound b, Depth d, Move m, Value ev, uint8_t gen) noexcept {
        // Only overwrite move if we have a valid move or the entry is for a different position
        if (m != Move::none() || static_cast<uint16_t>(k >> 48) != key16) {
            move16 = static_cast<uint16_t>(m.raw());
        }

        // Overwrite entry details
        key16    = static_cast<uint16_t>(k >> 48);
        value16  = static_cast<int16_t>(v);
        eval16   = static_cast<int16_t>(ev);
        depth8   = static_cast<int8_t>(d);
        genBound = static_cast<uint8_t>(gen | b);
    }

    uint16_t key16;
    uint16_t move16;
    int16_t  value16;
    int16_t  eval16;
    int8_t   depth8;
    uint8_t  genBound;
};
static_assert(sizeof(TTEntry) == 10, "TTEntry must be exactly 10 bytes");

// 32-byte Cluster fitting evenly in 64-byte cache lines
struct alignas(32) TTCluster {
    static constexpr int CLUSTER_SIZE = 3;
    TTEntry entries[CLUSTER_SIZE];
    uint8_t padding[2];
};
static_assert(sizeof(TTCluster) == 32, "TTCluster must be exactly 32 bytes");

class TranspositionTable {
public:
    TranspositionTable() noexcept = default;
    ~TranspositionTable() { free_table(); }

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    void resize(std::size_t mbSize) noexcept;
    void clear() noexcept;

    // Advances generation counter to age old search entries
    void new_search() noexcept { generation_ += 4; }

    [[nodiscard]] TTEntry* probe(Key key, bool& found) noexcept;
    [[nodiscard]] int hashfull() const noexcept;

    // Fast prefetch for Position::do_move
    void prefetch(Key key) const noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(&clusters_[index(key)]);
#elif defined(_MSC_VER)
        _mm_prefetch(reinterpret_cast<const char*>(&clusters_[index(key)]), _MM_HINT_T0);
#endif
    }

private:
    [[nodiscard]] std::size_t index(Key key) const noexcept {
#if defined(__GNUC__) || defined(__clang__)
        __extension__ typedef unsigned __int128 uint128_t;
        return static_cast<std::size_t>((static_cast<uint128_t>(key) * clusterCount_) >> 64);
#else
        return static_cast<std::size_t>(key % clusterCount_);
#endif
    }

    void free_table() noexcept;

    std::size_t clusterCount_{0};
    TTCluster* clusters_{nullptr};
    void* rawMemory_{nullptr};
    uint8_t generation_{0};
};

// Global Transposition Table instance
extern TranspositionTable TT;

} // namespace Engine::Search