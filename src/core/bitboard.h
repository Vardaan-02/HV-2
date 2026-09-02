#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <array>

#include "core/intrinsics.h"
#include "core/types.h"

namespace Engine::Core {

constexpr Bitboard FileABB = 0x0101010101010101ULL;
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;

constexpr Bitboard Rank1BB = 0xFFULL;
constexpr Bitboard Rank2BB = Rank1BB << (8 * 1);
constexpr Bitboard Rank3BB = Rank1BB << (8 * 2);
constexpr Bitboard Rank4BB = Rank1BB << (8 * 3);
constexpr Bitboard Rank5BB = Rank1BB << (8 * 4);
constexpr Bitboard Rank6BB = Rank1BB << (8 * 5);
constexpr Bitboard Rank7BB = Rank1BB << (8 * 6);
constexpr Bitboard Rank8BB = Rank1BB << (8 * 7);

struct Magic {
    Bitboard  mask{0};
    Bitboard* attacks{nullptr};
#if !defined(USE_PEXT)
    Bitboard  magic{0};
    unsigned  shift{0};
#endif

    [[nodiscard]] unsigned index(Bitboard occupied) const noexcept {
#if defined(USE_PEXT)
        return static_cast<unsigned>(pext64(occupied, mask));
#else
        return static_cast<unsigned>(((occupied & mask) * magic) >> shift);
#endif
    }

    [[nodiscard]] Bitboard attacks_bb(Bitboard occupied) const noexcept {
        return attacks[index(occupied)];
    }
};

extern uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard RayPassBB[SQUARE_NB][SQUARE_NB];
alignas(64) extern Magic Magics[SQUARE_NB][2];

[[nodiscard]] constexpr Bitboard square_bb(Square s) noexcept {
    assert(is_ok(s));
    return (1ULL << s);
}

[[nodiscard]] constexpr Bitboard  operator&(Bitboard b, Square s) noexcept { return b & square_bb(s); }
[[nodiscard]] constexpr Bitboard  operator|(Bitboard b, Square s) noexcept { return b | square_bb(s); }
[[nodiscard]] constexpr Bitboard  operator^(Bitboard b, Square s) noexcept { return b ^ square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) noexcept { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) noexcept { return b ^= square_bb(s); }

[[nodiscard]] constexpr Bitboard operator&(Square s, Bitboard b) noexcept { return b & s; }
[[nodiscard]] constexpr Bitboard operator|(Square s, Bitboard b) noexcept { return b | s; }
[[nodiscard]] constexpr Bitboard operator^(Square s, Bitboard b) noexcept { return b ^ s; }
[[nodiscard]] constexpr Bitboard operator|(Square s1, Square s2) noexcept { return square_bb(s1) | s2; }

[[nodiscard]] constexpr bool more_than_one(Bitboard b) noexcept { 
    return (b & (b - 1)) != 0; 
}

[[nodiscard]] constexpr Bitboard rank_bb(Rank r) noexcept { return Rank1BB << (8 * r); }
[[nodiscard]] constexpr Bitboard rank_bb(Square s) noexcept { return rank_bb(rank_of(s)); }
[[nodiscard]] constexpr Bitboard file_bb(File f) noexcept { return FileABB << f; }
[[nodiscard]] constexpr Bitboard file_bb(Square s) noexcept { return file_bb(file_of(s)); }

template<Direction D>
[[nodiscard]] constexpr Bitboard shift(Bitboard b) noexcept {
    if constexpr (D == NORTH)         return b << 8;
    if constexpr (D == SOUTH)         return b >> 8;
    if constexpr (D == NORTH + NORTH) return b << 16;
    if constexpr (D == SOUTH + SOUTH) return b >> 16;
    if constexpr (D == EAST)          return (b & ~FileHBB) << 1;
    if constexpr (D == WEST)          return (b & ~FileABB) >> 1;
    if constexpr (D == NORTH_EAST)    return (b & ~FileHBB) << 9;
    if constexpr (D == NORTH_WEST)    return (b & ~FileABB) << 7;
    if constexpr (D == SOUTH_EAST)    return (b & ~FileHBB) >> 7;
    if constexpr (D == SOUTH_WEST)    return (b & ~FileABB) >> 9;
    return 0;
}

template<Color C>
[[nodiscard]] constexpr Bitboard pawn_attacks_bb(Bitboard b) noexcept {
    if constexpr (C == WHITE) {
        return shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b);
    } else {
        return shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
    }
}

[[nodiscard]] inline Bitboard line_bb(Square s1, Square s2) noexcept {
    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}

[[nodiscard]] inline Bitboard between_bb(Square s1, Square s2) noexcept {
    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}

template<typename T = Square>
[[nodiscard]] inline int distance(Square x, Square y) noexcept;

template<>
[[nodiscard]] inline int distance<File>(Square x, Square y) noexcept {
    return std::abs(static_cast<int>(file_of(x)) - static_cast<int>(file_of(y)));
}

template<>
[[nodiscard]] inline int distance<Rank>(Square x, Square y) noexcept {
    return std::abs(static_cast<int>(rank_of(x)) - static_cast<int>(rank_of(y)));
}

template<>
[[nodiscard]] inline int distance<Square>(Square x, Square y) noexcept {
    return SquareDistance[x][y];
}

[[nodiscard]] inline int edge_distance(File f) noexcept { 
    return std::min(static_cast<int>(f), static_cast<int>(FILE_H - f)); 
}

[[nodiscard]] constexpr int popcount(Bitboard b) noexcept {
    return popcount64(b);
}

[[nodiscard]] constexpr Square lsb(Bitboard b) noexcept {
    assert(b != 0);
    return static_cast<Square>(count_trailing_zeros64(b));
}

[[nodiscard]] constexpr Square msb(Bitboard b) noexcept {
    assert(b != 0);
    return static_cast<Square>(63 - std::countl_zero(b));
}

[[nodiscard]] constexpr Bitboard least_significant_square_bb(Bitboard b) noexcept {
    assert(b != 0);
    return b & -b;
}

inline Square pop_lsb(Bitboard& b) noexcept {
    assert(b != 0);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

namespace Bitboards {

void init() noexcept;
[[nodiscard]] std::string pretty(Bitboard b);

[[nodiscard]] constexpr int const_abs(int v) noexcept {
    return v < 0 ? -v : v;
}

[[nodiscard]] constexpr Bitboard safe_destination(Square s, int step) noexcept {
    const int dest = static_cast<int>(s) + step;
    if (dest < SQ_A1 || dest > SQ_H8)
        return 0;
    const Square to = static_cast<Square>(dest);
    const int fileDiff = const_abs(static_cast<int>(file_of(s)) - static_cast<int>(file_of(to)));
    return fileDiff <= 2 ? square_bb(to) : 0;
}

[[nodiscard]] constexpr Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) noexcept {
    Bitboard attacks = 0;

    constexpr std::array<Direction, 4> rookDirs   = {NORTH, SOUTH, EAST, WEST};
    constexpr std::array<Direction, 4> bishopDirs = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

    const auto& dirs = (pt == ROOK) ? rookDirs : bishopDirs;

    for (const Direction d : dirs) {
        Square s = sq;
        while (safe_destination(s, d)) {
            s += d;
            attacks |= s;
            if (occupied & s)
                break;
        }
    }
    return attacks;
}

[[nodiscard]] constexpr Bitboard knight_attack(Square sq) noexcept {
    Bitboard b = 0;
    constexpr std::array<int, 8> steps = {-17, -15, -10, -6, 6, 10, 15, 17};
    for (const int step : steps)
        b |= safe_destination(sq, step);
    return b;
}

[[nodiscard]] constexpr Bitboard king_attack(Square sq) noexcept {
    Bitboard b = 0;
    constexpr std::array<int, 8> steps = {-9, -8, -7, -1, 1, 7, 8, 9};
    for (const int step : steps)
        b |= safe_destination(sq, step);
    return b;
}

[[nodiscard]] constexpr Bitboard pseudo_attacks(PieceType pt, Square sq) noexcept {
    switch (pt) {
        case ROOK:   return sliding_attack(ROOK, sq, 0);
        case BISHOP: return sliding_attack(BISHOP, sq, 0);
        case QUEEN:  return sliding_attack(ROOK, sq, 0) | sliding_attack(BISHOP, sq, 0);
        case KNIGHT: return knight_attack(sq);
        case KING:   return king_attack(sq);
        default:     return 0;
    }
}

} // namespace Bitboards

inline constexpr auto PseudoAttacks = []() constexpr {
    std::array<std::array<Bitboard, SQUARE_NB>, PIECE_TYPE_NB> attacks{};
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
        attacks[WHITE][s1]  = pawn_attacks_bb<WHITE>(square_bb(s1));
        attacks[BLACK][s1]  = pawn_attacks_bb<BLACK>(square_bb(s1));
        attacks[KING][s1]   = Bitboards::pseudo_attacks(KING, s1);
        attacks[KNIGHT][s1] = Bitboards::pseudo_attacks(KNIGHT, s1);
        attacks[BISHOP][s1] = Bitboards::pseudo_attacks(BISHOP, s1);
        attacks[ROOK][s1]   = Bitboards::pseudo_attacks(ROOK, s1);
        attacks[QUEEN][s1]  = attacks[BISHOP][s1] | attacks[ROOK][s1];
    }
    return attacks;
}();

template<PieceType Pt>
[[nodiscard]] inline Bitboard attacks_bb(Square s, Color c = COLOR_NB) noexcept {
    assert((Pt != PAWN || c < COLOR_NB) && is_ok(s));
    return Pt == PAWN ? PseudoAttacks[c][s] : PseudoAttacks[Pt][s];
}

template<PieceType Pt>
[[nodiscard]] inline Bitboard attacks_bb(Square s, Bitboard occupied) noexcept {
    assert(Pt != PAWN && is_ok(s));
    switch (Pt) {
        case BISHOP:
        case ROOK:
            return Magics[s][Pt - BISHOP].attacks_bb(occupied);
        case QUEEN:
            return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
        default:
            return PseudoAttacks[Pt][s];
    }
}

[[nodiscard]] inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) noexcept {
    assert(pt != PAWN && is_ok(s));
    switch (pt) {
        case BISHOP: return attacks_bb<BISHOP>(s, occupied);
        case ROOK:   return attacks_bb<ROOK>(s, occupied);
        case QUEEN:  return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
        default:     return PseudoAttacks[pt][s];
    }
}

[[nodiscard]] inline Bitboard attacks_bb(Piece pc, Square s) noexcept {
    const PieceType pt = type_of(pc);
    return pt == PAWN ? PseudoAttacks[color_of(pc)][s] : PseudoAttacks[pt][s];
}

[[nodiscard]] inline Bitboard attacks_bb(Piece pc, Square s, Bitboard occupied) noexcept {
    const PieceType pt = type_of(pc);
    return pt == PAWN ? PseudoAttacks[color_of(pc)][s] : attacks_bb(pt, s, occupied);
}

} // namespace Engine::Core