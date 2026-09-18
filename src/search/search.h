#pragma once

#include <chrono>
#include <atomic>

#include "core/types.h"
#include "position/position.h"
#include "search/tt.h"
#include "search/history.h"
#include "movegen/movepicker.h"

namespace Engine::Search {

struct SearchLimits {
    int depth{Core::MAX_PLY};
    int64_t moveTime{-1};
    int64_t time[Core::COLOR_NB]{-1, -1};
    int64_t inc[Core::COLOR_NB]{0, 0};
    int movesToGo{-1};
    uint64_t nodes{0};
    bool infinite{false};
};

struct Stack {
    Core::Move currentMove{Core::Move::none()};
    Core::Move killers[2]{Core::Move::none(), Core::Move::none()};
    Core::Value staticEval{Core::VALUE_NONE};
    int ply{0};
};

class Searcher {
public:
    Searcher() = default;

    void start_search(Position::Position& pos, const SearchLimits& limits) noexcept;
    void stop() noexcept { stopRequested_.store(true, std::memory_order_relaxed); }

private:
    Core::Value pvs(Position::Position& pos, Core::Value alpha, Core::Value beta, Core::Depth depth, Stack* ss) noexcept;
    Core::Value qsearch(Position::Position& pos, Core::Value alpha, Core::Value beta, Stack* ss) noexcept;

    bool should_stop() noexcept;

    SearchLimits limits_;
    std::chrono::steady_clock::time_point startTime_;
    std::atomic<bool> stopRequested_{false};
    uint64_t nodes_{0};

    ButterflyHistory history_;
    CounterMoveTable counterMoves_;
    KillerTable killers_;
};

} // namespace Engine::Search