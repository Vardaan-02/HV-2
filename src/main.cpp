#include <iostream>
#include <cassert>

#include "core/bitboard.h"
#include "position/position.h"
#include "movegen/perft.h"
#include "search/tt.h"
#include "search/history.h"

int main() {
    Engine::Core::Bitboards::init();
    Engine::Position::Position::init();

    std::cout << "Initializing Transposition Table (64 MB)...\n";
    Engine::Search::TT.resize(64);

    // TT probe verification
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

    // History Table verification
    Engine::Search::ButterflyHistory history;
    history.clear();
    history.update(Engine::Core::WHITE, Engine::Core::SQ_E2, Engine::Core::SQ_E4, 300);
    assert(history.get(Engine::Core::WHITE, Engine::Core::SQ_E2, Engine::Core::SQ_E4) > 0);

    std::cout << "TT and History tables initialized cleanly.\n\n";

    return Engine::MoveGen::run_full_perft_suite() ? 0 : 1;
}