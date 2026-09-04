#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "movegen/movegen.h"
#include "position/position.h"

namespace Engine::MoveGen {

using Core::Depth;
using Core::Move;
using Position = Engine::Position::Position;
using StateInfo = Engine::Position::StateInfo;

// Recursive perft counter with bulk-counting leaf optimization
template<bool Root = false>
inline uint64_t perft(Position& pos, Depth depth) noexcept {
    assert(depth >= 0);

    if (depth == 0) {
        return 1ULL;
    }

    StateInfo st;
    uint64_t nodes = 0;
    const MoveList<LEGAL> moveList(pos);

    if (depth == 1) {
        return moveList.size();
    }

    for (const Move m : moveList) {
        pos.do_move(m, st);
        nodes += perft<false>(pos, depth - 1);
        pos.undo_move(m);
    }

    return nodes;
}

// UCI Divide: Prints move-by-move node counts with telemetry
inline uint64_t perft_divide(Position& pos, Depth depth) noexcept {
    if (depth <= 0) {
        return 1ULL;
    }

    StateInfo st;
    uint64_t totalNodes = 0;
    const auto startTime = std::chrono::steady_clock::now();

    std::cout << "\n==========================================\n";
    std::cout << "         PERFT DIVIDE (DEPTH " << depth << ")\n";
    std::cout << "==========================================\n";

    const MoveList<LEGAL> moveList(pos);
    for (const Move m : moveList) {
        pos.do_move(m, st);
        const uint64_t nodes = (depth == 1) ? 1ULL : perft<false>(pos, depth - 1);
        pos.undo_move(m);

        totalNodes += nodes;

        const auto from = m.from_sq();
        const auto to   = m.to_sq();
        std::cout << static_cast<char>('a' + Core::file_of(from))
                  << static_cast<char>('1' + Core::rank_of(from))
                  << static_cast<char>('a' + Core::file_of(to))
                  << static_cast<char>('1' + Core::rank_of(to));

        if (m.type_of() == Core::PROMOTION) {
            constexpr char promoChars[] = {' ', ' ', 'n', 'b', 'r', 'q'};
            std::cout << promoChars[m.promotion_type()];
        }

        std::cout << ": " << nodes << "\n";
    }

    const auto endTime = std::chrono::steady_clock::now();
    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    const double elapsedSec = static_cast<double>(elapsedUs) / 1'000'000.0;
    const double nps = (elapsedSec > 0.0) ? (static_cast<double>(totalNodes) / elapsedSec) : 0.0;

    std::cout << "------------------------------------------\n";
    std::cout << "Total Nodes : " << totalNodes << "\n";
    std::cout << "Time Taken  : " << std::fixed << std::setprecision(2) << (static_cast<double>(elapsedUs) / 1000.0) << " ms\n";
    std::cout << "Speed       : " << std::fixed << std::setprecision(2) << (nps / 1'000'000.0) << " MNPS ("
              << static_cast<uint64_t>(nps) << " NPS)\n";
    std::cout << "==========================================\n";

    return totalNodes;
}

struct PerftTestCase {
    std::string_view name;
    std::string_view fen;
    Depth depth;
    uint64_t expectedNodes;
};

inline const std::vector<PerftTestCase> FullWikiPerftSuites = {
    // 1. Initial Standard Position
    {
        "CPW Pos 1 (Initial)",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        6,
        119'060'324ULL
    },
    // 2. Kiwipete by Peter McKenzie (Complex pin/castling/promotion density)
    {
        "CPW Pos 2 (Kiwipete)",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        5,
        193'690'690ULL
    },
    // 3. Position 3 (Endgame pawn promotion, discovered check blocks)
    {
        "CPW Pos 3 (Endgame)",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        7,
        178'633'661ULL
    },
    // 4. Position 4 / Mirrored Talkchess position (Double checks, promotions)
    {
        "CPW Pos 4 (Talkchess)",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        6,
        706'045'033ULL
    },
    // 4b. Position 4 Mirrored (Black's perspective)
    {
        "CPW Pos 4 (Mirrored)",
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
        6,
        706'045'033ULL
    },
    // 5. Position 5 (Heavy center pawn tension, king mobility under threat)
    {
        "CPW Pos 5",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        5,
        89'941'194ULL
    },
    // 6. Position 6 (Alternative CPW standard benchmark)
    {
        "CPW Pos 6",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        5,
        164'075'551ULL
    },
    // 7. En Passant Pin Edge Case (Horizontal pin prevents c4xd3)
    // Stockfish verified: D1=6, D2=95, D3=2217, D4=39268, D5=978740, D6=1182387
    {
        "En Passant Pin",
        "8/8/8/8/k1pP3R/8/8/4K3 b - d3 0 1",
        6,
        1'182'387ULL
    },
    // 8. Castling Check Test (King cannot castle through attacked squares)
    // FEN: r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
    // Depth 1: 26, Depth 2: 568, Depth 3: 13744, Depth 4: 314346
    {
        "Castling Check Test",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        4,
        314'346ULL
    }
};

inline bool run_full_perft_suite() noexcept {
    Position pos;
    StateInfo st;
    bool allPassed = true;

    uint64_t grandTotalNodes = 0;
    int64_t grandTotalUs = 0;

    std::cout << "\n====================================================================================================\n";
    std::cout << "                              FULL CHESS PROGRAMMING WIKI PERFT SUITE                               \n";
    std::cout << "====================================================================================================\n";
    std::cout << std::left 
              << std::setw(6)  << "Status"
              << std::setw(25) << "Suite Name"
              << std::setw(7)  << "Depth"
              << std::setw(14) << "Nodes"
              << std::setw(13) << "Time"
              << std::setw(14) << "Speed (MNPS)"
              << "NPS\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";

    for (const auto& test : FullWikiPerftSuites) {
        pos.set(test.fen, false, &st);

        const auto startTime = std::chrono::steady_clock::now();
        const uint64_t actual = perft<true>(pos, test.depth);
        const auto endTime = std::chrono::steady_clock::now();

        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        const double elapsedSec = static_cast<double>(elapsedUs) / 1'000'000.0;
        const double nps = (elapsedSec > 0.0) ? (static_cast<double>(actual) / elapsedSec) : 0.0;
        const double mnps = nps / 1'000'000.0;

        const bool passed = (actual == test.expectedNodes);
        if (!passed) allPassed = false;

        grandTotalNodes += actual;
        grandTotalUs += elapsedUs;

        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " 
                  << std::left << std::setw(25) << test.name
                  << std::setw(7)  << test.depth
                  << std::setw(14) << actual
                  << std::fixed << std::setprecision(2) << std::setw(9) << (static_cast<double>(elapsedUs) / 1000.0) << " ms "
                  << std::setw(14) << mnps
                  << static_cast<uint64_t>(nps) << "\n";
    }

    const double totalSec = static_cast<double>(grandTotalUs) / 1'000'000.0;
    const double avgNps = (totalSec > 0.0) ? (static_cast<double>(grandTotalNodes) / totalSec) : 0.0;

    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "SUMMARY:\n";
    std::cout << "  Total Nodes Traversed : " << grandTotalNodes << "\n";
    std::cout << "  Total Time Elapsed    : " << std::fixed << std::setprecision(2) << (static_cast<double>(grandTotalUs) / 1000.0) << " ms ("
              << totalSec << " s)\n";
    std::cout << "  Overall Engine NPS    : " << static_cast<uint64_t>(avgNps) << " (" 
              << std::fixed << std::setprecision(2) << (avgNps / 1'000'000.0) << " MNPS)\n";
    std::cout << "====================================================================================================\n";
    std::cout << (allPassed ? ">> ALL 9 PERFT SUITES PASSED!\n" : ">> SOME SUITES FAILED!\n");
    std::cout << "====================================================================================================\n\n";

    return allPassed;
}

} // namespace Engine::MoveGen