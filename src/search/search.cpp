#include "search/search.h"
#include "search/tt.h"
#include <chrono>
#include <iostream>
#include <algorithm>

namespace Engine::Search {

namespace {

// Material evaluation placeholder until Phase 5 (Eval Module)
Core::Value static_evaluate(const Position::Position& pos) noexcept {
    using namespace Core;

    int score = (pos.count<PAWN>(WHITE)   - pos.count<PAWN>(BLACK))   * 100
              + (pos.count<KNIGHT>(WHITE) - pos.count<KNIGHT>(BLACK)) * 300
              + (pos.count<BISHOP>(WHITE) - pos.count<BISHOP>(BLACK)) * 310
              + (pos.count<ROOK>(WHITE)   - pos.count<ROOK>(BLACK))   * 500
              + (pos.count<QUEEN>(WHITE)  - pos.count<QUEEN>(BLACK))  * 900;

    return pos.side_to_move() == WHITE ? static_cast<Value>(score) 
                                       : static_cast<Value>(-score);
}

} // namespace

int64_t allocate_time(const SearchLimits& limits, Core::Color us) noexcept {
    if (limits.moveTime > 0) {
        return limits.moveTime;
    }

    int64_t timeRemaining = limits.time[us];
    int64_t increment = limits.inc[us];

    if (timeRemaining <= 0) return 100; // Default fallback (100ms)

    int movesToGo = (limits.movesToGo > 0) ? limits.movesToGo : 30;
    int64_t targetTime = (timeRemaining / movesToGo) + (increment * 3 / 4);

    return std::min(targetTime, static_cast<int64_t>(timeRemaining * 0.8));
}

bool Searcher::should_stop() noexcept {
    if (stopRequested_.load(std::memory_order_relaxed)) return true;

    // Periodically check elapsed time every 2048 nodes if time limit is active
    if ((nodes_ & 2047) == 0 && (limits_.moveTime > 0 || limits_.time[Core::WHITE] > 0 || limits_.time[Core::BLACK] > 0)) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
        if (elapsed >= allocatedTimeMs_) {
            stopRequested_.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

void Searcher::start_search(Position::Position& pos, const SearchLimits& limits) noexcept {
    limits_ = limits;
    stopRequested_.store(false, std::memory_order_relaxed);
    nodes_ = 0;
    startTime_ = std::chrono::steady_clock::now();
    allocatedTimeMs_ = allocate_time(limits, pos.side_to_move());

    TT.new_search(); // Increment TT generation counter

    Core::Move bestMove = Core::Move::none();
    Core::Move previousBestMove = Core::Move::none();
    Core::Value bestScore = -Core::VALUE_INFINITE;

    Stack stack[Core::MAX_PLY + 1];
    for (int i = 0; i <= Core::MAX_PLY; ++i) {
        stack[i].ply = i;
        stack[i].currentMove = Core::Move::none();
    }

    const int maxDepth = (limits_.depth > 0) ? std::min(limits_.depth, Core::MAX_PLY) : Core::MAX_PLY;

    // Iterative Deepening Loop
    for (Core::Depth depth = 1; depth <= maxDepth; ++depth) {
        Core::Value score = pvs(pos, -Core::VALUE_INFINITE, Core::VALUE_INFINITE, depth, stack);

        if (should_stop()) break;

        bestScore = score;
        
        // Retrieve best move found from TT for current root depth
        bool found = false;
        TTEntry* tte = TT.probe(pos.key(), found);
        if (found && tte->move() != Core::Move::none()) {
            bestMove = tte->move();
            previousBestMove = bestMove;
        }

        // UCI Info output
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
        uint64_t nps = (elapsedMs > 0) ? (nodes_ * 1000) / elapsedMs : 0;

        std::cout << "info depth " << depth
                  << " score cp " << bestScore
                  << " nodes " << nodes_
                  << " nps " << nps
                  << " time " << elapsedMs
                  << " hashfull " << TT.hashfull()
                  << " pv " << bestMove.to_string()
                  << std::endl;
    }

    // Fall back to previous iterative best if search stopped mid-depth
    if (bestMove == Core::Move::none()) {
        bestMove = previousBestMove;
    }

    std::cout << "bestmove " << bestMove.to_string() << std::endl;
}

Core::Value Searcher::qsearch(Position::Position& pos, Core::Value alpha, Core::Value beta, Stack* ss) noexcept {
    ++nodes_;
    
    if (should_stop()) return Core::VALUE_ZERO;

    Core::Value standPat = static_evaluate(pos);
    if (standPat >= beta) return beta;
    if (alpha < standPat) alpha = standPat;

    MoveGen::MovePicker mp(pos, Core::Move::none(), &history_);
    Core::Move move;

    Position::StateInfo st;
    while ((move = mp.next_move()) != Core::Move::none()) {
        if (!pos.legal(move)) continue;

        pos.do_move(move, st);
        Core::Value score = -qsearch(pos, -beta, -alpha, ss + 1);
        pos.undo_move(move);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

Core::Value Searcher::pvs(Position::Position& pos, Core::Value alpha, Core::Value beta, Core::Depth depth, Stack* ss) noexcept {
    if (depth <= 0) return qsearch(pos, alpha, beta, ss);

    ++nodes_;
    if (should_stop()) return Core::VALUE_ZERO;

    bool pvNode = (beta - alpha > 1);
    bool found = false;
    Core::Move ttMove = Core::Move::none();
    
    // Probe retrieves a direct hit or selects a replacement slot
    TTEntry* tte = TT.probe(pos.key(), found);
    if (found) {
        ttMove = tte->move();
        if (!pvNode && tte->depth() >= depth) {
            Core::Value ttVal = tte->value();
            if (tte->bound() == BOUND_EXACT) return ttVal;
            if (tte->bound() == BOUND_LOWER && ttVal >= beta) return ttVal;
            if (tte->bound() == BOUND_UPPER && ttVal <= alpha) return ttVal;
        }
    }

    MoveGen::MovePicker mp(pos, ttMove, depth, &history_, &counterMoves_, &killers_, ss->ply);
    Core::Move move;
    Core::Value bestScore = -Core::VALUE_INFINITE;
    Core::Move bestMove = Core::Move::none();
    int movesTried = 0;

    Position::StateInfo st;
    while ((move = mp.next_move()) != Core::Move::none()) {
        if (!pos.legal(move)) continue;

        ++movesTried;
        ss->currentMove = move;
        pos.do_move(move, st);

        Core::Value score;
        if (movesTried == 1) {
            score = -pvs(pos, -beta, -alpha, depth - 1, ss + 1);
        } else {
            // Zero-window search for late moves
            score = -pvs(pos, -alpha - 1, -alpha, depth - 1, ss + 1);
            if (score > alpha && score < beta) {
                // Re-search full window if fail-high
                score = -pvs(pos, -beta, -alpha, depth - 1, ss + 1);
            }
        }

        pos.undo_move(move);

        if (should_stop()) return Core::VALUE_ZERO;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;

            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) {
                    if (!pos.capture(move)) {
                        killers_.update(ss->ply, move);
                        history_.update(pos.side_to_move(), move.from_sq(), move.to_sq(), depth);
                    }
                    break; // Alpha-beta cutoff
                }
            }
        }
    }

    if (movesTried == 0) {
        return pos.checkers() ? -Core::VALUE_MATE + ss->ply : Core::VALUE_DRAW;
    }

    Bound bound = (bestScore >= beta) ? BOUND_LOWER : (bestScore > alpha ? BOUND_EXACT : BOUND_UPPER);

    tte->save(pos.key(), bestScore, bound, depth, bestMove, Core::VALUE_NONE, TT.generation());

    return bestScore;
}

} // namespace Engine::Search