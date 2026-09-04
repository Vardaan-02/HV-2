#include <iostream>
#include <cassert>

#include "core/bitboard.h"
#include "position/position.h"
#include "movegen/perft.h"
#include "search/tt.h"

int main() {
    // 1. Initialize bitboard tables, Zobrist keys, and repetition tables
    Engine::Core::Bitboards::init();
    Engine::Position::Position::init();

    // 2. Initialize and verify Transposition Table (64 MB)
    std::cout << "Initializing Transposition Table (64 MB)...\n";
    Engine::Search::TT.resize(64);

    // Basic TT sanity test
    bool found = false;
    Engine::Core::Key testKey = 0x123456789ABCDEF0ULL;
    Engine::Search::TTEntry* entry = Engine::Search::TT.probe(testKey, found);
    assert(!found);

    entry->save(testKey, static_cast<Engine::Core::Value>(150),
                Engine::Search::BOUND_EXACT, 8,
                Engine::Core::Move::make<Engine::Core::NORMAL>(Engine::Core::SQ_E2, Engine::Core::SQ_E4),
                static_cast<Engine::Core::Value>(140), 0);

    Engine::Search::TTEntry* probed = Engine::Search::TT.probe(testKey, found);
    assert(found);
    assert(probed->value() == 150);
    assert(probed->depth() == 8);
    assert(probed->bound() == Engine::Search::BOUND_EXACT);

    std::cout << "TT sanity check passed. Hashfull: " << Engine::Search::TT.hashfull() << " permille.\n\n";

    // 3. Run full perft suite to ensure wiring caused zero regressions
    return Engine::MoveGen::run_full_perft_suite() ? 0 : 1;
}