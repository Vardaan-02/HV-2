#include "search/tt.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

namespace Engine::Search {

TranspositionTable TT;

void TranspositionTable::resize(std::size_t mbSize) noexcept {
    free_table();

    const std::size_t totalBytes = mbSize * 1024 * 1024;
    clusterCount_ = totalBytes / sizeof(TTCluster);

    if (clusterCount_ == 0) {
        clusterCount_ = 1;
    }

    // Allocate cache-line aligned memory (64-byte alignment)
    const std::size_t allocBytes = clusterCount_ * sizeof(TTCluster) + 64;
    rawMemory_ = std::malloc(allocBytes);

    if (!rawMemory_) {
        std::cerr << "Failed to allocate " << mbSize << " MB for Transposition Table!" << std::endl;
        std::abort();
    }

    // Align pointer to 64-byte boundary
    std::size_t space = allocBytes;
    void* ptr = rawMemory_;
    clusters_ = static_cast<TTCluster*>(std::align(64, clusterCount_ * sizeof(TTCluster), ptr, space));

    clear();
}

void TranspositionTable::clear() noexcept {
    if (clusters_) {
        std::memset(clusters_, 0, clusterCount_ * sizeof(TTCluster));
    }
    generation_ = 0;
}

void TranspositionTable::free_table() noexcept {
    if (rawMemory_) {
        std::free(rawMemory_);
        rawMemory_ = nullptr;
        clusters_ = nullptr;
        clusterCount_ = 0;
    }
}

TTEntry* TranspositionTable::probe(Key key, bool& found) noexcept {
    assert(clusters_ != nullptr);

    TTCluster& cluster = clusters_[index(key)];
    const uint16_t targetKey = static_cast<uint16_t>(key >> 48);

    // 1. Direct hit check
    for (int i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].key16 == targetKey || !cluster.entries[i].genBound) {
            found = (cluster.entries[i].genBound != 0);
            return &cluster.entries[i];
        }
    }

    // 2. Replacement Policy: Find the least valuable entry in the cluster
    // Priority: Replace entries from older generations, or with shallower depths
    found = false;
    TTEntry* replace = &cluster.entries[0];

    for (int i = 1; i < TTCluster::CLUSTER_SIZE; ++i) {
        TTEntry& candidate = cluster.entries[i];

        // Lower score = better candidate to replace
        const int candScore = (candidate.generation() == generation_ ? 0 : 256) - candidate.depth();
        const int bestScore = (replace->generation() == generation_ ? 0 : 256) - replace->depth();

        if (candScore < bestScore) {
            replace = &candidate;
        }
    }

    return replace;
}

// Permille (parts per 1000) of TT slots occupied by the current generation
int TranspositionTable::hashfull() const noexcept {
    if (!clusters_ || clusterCount_ == 0) return 0;

    int occupied = 0;
    constexpr int SAMPLES = 1000;
    const std::size_t step = std::max<std::size_t>(1, clusterCount_ / SAMPLES);

    for (std::size_t i = 0; i < clusterCount_; i += step) {
        for (int j = 0; j < TTCluster::CLUSTER_SIZE; ++j) {
            if (clusters_[i].entries[j].genBound && clusters_[i].entries[j].generation() == generation_) {
                ++occupied;
            }
        }
    }

    return (occupied * 1000) / (SAMPLES * TTCluster::CLUSTER_SIZE);
}

} // namespace Engine::Search