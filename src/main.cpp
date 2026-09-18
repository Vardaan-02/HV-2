#include <iostream>
#include <cassert>

#include "core/bitboard.h"
#include "position/position.h"
#include "movegen/perft.h"
#include "movegen/movepicker.h"
#include "search/tt.h"
#include "search/history.h"

int main() {
    Engine::Core::Bitboards::init();
    Engine::Position::Position::init();

    std::cout << "Initializing Transposition Table (64 MB)...\n";
    Engine::Search::TT.resize(64);

    // TT verification
    bool found = false;
    const Engine::Core::Key testKey = 0x123456789ABCDEF0ULL;
    Engine::Search::TTEntry* entry = Engine::Search::TT.probe(testKey, found);
    assert(!found);

    entry->save(testKey, static_cast<Engine::Core::Value>(150),
                Engine::Search::BOUND_EXACT, 8,
                Engine::Core::Move::make<Engine::Core::NORMAL>(Engine::Core::SQ_E2, Engine::Core::SQ_E4),
                static_cast<Engine::Core::Value>(140), 0);

    const Engine::Search::TTEntry* probed = Engine::Search::TT.probe(testKey, found);
    assert(found);
    assert(probed->value() == 150);
    assert(probed->depth() == 8);
    assert(probed->bound() == Engine::Search::BOUND_EXACT);
    (void)probed;

    // History and MovePicker verification
    Engine::Search::ButterflyHistory history;
    history.clear();
    Engine::Search::CounterMoveTable counterMoves;
    counterMoves.clear();
    Engine::Search::KillerTable killers;
    killers.clear();

    Engine::Position::Position pos;
    Engine::Position::StateInfo st;
    pos.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", false, &st);

    const Engine::Core::Move ttMove = Engine::Core::Move::make<Engine::Core::NORMAL>(Engine::Core::SQ_E2, Engine::Core::SQ_E4);
    Engine::MoveGen::MovePicker mp(pos, ttMove, 4, &history, &counterMoves, &killers, 0);

    const Engine::Core::Move firstMove = mp.next_move();
    assert(firstMove == ttMove);
    (void)firstMove; // Silences -Wunused-but-set-variable

    int moveCount = 1;
    while (mp.next_move() != Engine::Core::Move::none()) {
        ++moveCount;
    }
    assert(moveCount == 20);

    std::cout << "TT, History, and MovePicker verified (" << moveCount << " moves picked in order).\n\n";

    return Engine::MoveGen::run_full_perft_suite() ? 0 : 1;
}