#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "core/types.h"

namespace Engine::Search {

using Core::Color;
using Core::Depth;
using Core::Move;
using Core::Piece;
using Core::Square;

constexpr int16_t HISTORY_MAX = 16384;

// Gravity-based bonus update: prevents saturating 16-bit bounds
inline void update_history_score(int16_t& val, int bonus) noexcept {
    val += bonus - (static_cast<int>(val) * std::abs(bonus)) / HISTORY_MAX;
}

// Butterfly History Table: [Color][FromSquare][ToSquare]
class ButterflyHistory {
public:
    void clear() noexcept {
        table_.fill(0);
    }

    [[nodiscard]] int16_t get(Color c, Square from, Square to) const noexcept {
        return table_[index(c, from, to)];
    }

    void update(Color c, Square from, Square to, int bonus) noexcept {
        update_history_score(table_[index(c, from, to)], bonus);
    }

private:
    [[nodiscard]] static constexpr std::size_t index(Color c, Square from, Square to) noexcept {
        return (static_cast<std::size_t>(c) * 64 * 64) +
               (static_cast<std::size_t>(from) * 64) +
                static_cast<std::size_t>(to);
    }

    std::array<int16_t, 2 * 64 * 64> table_{};
};

// Countermove Table: [PreviousPiece][PreviousToSquare] -> Best Response Move
class CounterMoveTable {
public:
    void clear() noexcept {
        table_.fill(Move::none());
    }

    [[nodiscard]] Move get(Piece prevPiece, Square prevTo) const noexcept {
        if (prevPiece == Core::NO_PIECE || prevTo == Core::SQ_NONE) {
            return Move::none();
        }
        return table_[index(prevPiece, prevTo)];
    }

    void update(Piece prevPiece, Square prevTo, Move refutation) noexcept {
        if (prevPiece != Core::NO_PIECE && prevTo != Core::SQ_NONE) {
            table_[index(prevPiece, prevTo)] = refutation;
        }
    }

private:
    [[nodiscard]] static constexpr std::size_t index(Piece p, Square sq) noexcept {
        return (static_cast<std::size_t>(p) * 64) + static_cast<std::size_t>(sq);
    }

    std::array<Move, Core::PIECE_NB * 64> table_{};
};

// Killer Moves Table: 2 slots per ply
class KillerTable {
public:
    void clear() noexcept {
        primary_.fill(Move::none());
        secondary_.fill(Move::none());
    }

    [[nodiscard]] Move primary(int ply) const noexcept {
        return (ply < MAX_PLY) ? primary_[ply] : Move::none();
    }

    [[nodiscard]] Move secondary(int ply) const noexcept {
        return (ply < MAX_PLY) ? secondary_[ply] : Move::none();
    }

    void update(int ply, Move m) noexcept {
        if (ply >= MAX_PLY || primary_[ply] == m) {
            return;
        }
        secondary_[ply] = primary_[ply];
        primary_[ply]   = m;
    }

private:
    static constexpr int MAX_PLY = 128;
    std::array<Move, MAX_PLY> primary_{};
    std::array<Move, MAX_PLY> secondary_{};
};

// Static Exchange Evaluation (SEE) piece values for fast capture heuristics
constexpr int SEE_VALUES[Core::PIECE_TYPE_NB] = {
    0,     // NO_PIECE_TYPE
    100,   // PAWN
    320,   // KNIGHT
    330,   // BISHOP
    500,   // ROOK
    900,   // QUEEN
    20000  // KING
};

} // namespace Engine::Search