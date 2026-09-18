#include "search/search.h"
#include <iostream>

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

bool Searcher::should_stop() noexcept {
    if (stopRequested_.load(std::memory_order_relaxed)) return true;
    if ((nodes_ & 2047) == 0 && limits_.moveTime > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        if (elapsed >= limits_.moveTime) {
            stopRequested_.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
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

void Searcher::start_search(Position::Position& pos, const SearchLimits& limits) noexcept {
    limits_ = limits;
    startTime_ = std::chrono::steady_clock::now();
    stopRequested_.store(false);
    nodes_ = 0;

    Stack stack[Core::MAX_PLY];
    for (int i = 0; i < Core::MAX_PLY; ++i) stack[i].ply = i;

    Core::Value alpha = -Core::VALUE_INFINITE;
    Core::Value beta = Core::VALUE_INFINITE;

    for (Core::Depth d = 1; d <= limits_.depth; ++d) {
        Core::Value score = pvs(pos, alpha, beta, d, stack);
        if (stopRequested_.load()) break;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime_).count();

        std::cout << "info depth " << d 
                  << " score cp " << score 
                  << " nodes " << nodes_ 
                  << " time " << elapsed << std::endl;
    }
}

} // namespace Engine::Search