#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Engine::Core {

using Key      = uint64_t;
using Bitboard = uint64_t;

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

enum Color : uint8_t {
    WHITE,
    BLACK,
    COLOR_NB = 2
};

enum CastlingRights : uint8_t {
    NO_CASTLING,
    WHITE_OO,
    WHITE_OOO = WHITE_OO << 1,
    BLACK_OO  = WHITE_OO << 2,
    BLACK_OOO = WHITE_OO << 3,

    KING_SIDE      = WHITE_OO | BLACK_OO,
    QUEEN_SIDE     = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHT_NB = 16
};

enum Bound : uint8_t {
    BOUND_NONE,
    BOUND_UPPER,
    BOUND_LOWER,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

using Value = int;

constexpr Value VALUE_ZERO     = 0;
constexpr Value VALUE_DRAW     = 0;
constexpr Value VALUE_NONE     = 32002;
constexpr Value VALUE_INFINITE = 32001;

constexpr Value VALUE_MATE             = 32000;
constexpr Value VALUE_MATE_IN_MAX_PLY  = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

constexpr Value VALUE_TB                 = VALUE_MATE_IN_MAX_PLY - 1;
constexpr Value VALUE_TB_WIN_IN_MAX_PLY  = VALUE_TB - MAX_PLY;
constexpr Value VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY;

[[nodiscard]] constexpr bool is_valid(Value value) noexcept { 
    return value != VALUE_NONE; 
}

[[nodiscard]] constexpr bool is_win(Value value) noexcept {
    assert(is_valid(value));
    return value >= VALUE_TB_WIN_IN_MAX_PLY;
}

[[nodiscard]] constexpr bool is_loss(Value value) noexcept {
    assert(is_valid(value));
    return value <= VALUE_TB_LOSS_IN_MAX_PLY;
}

[[nodiscard]] constexpr bool is_decisive(Value value) noexcept { 
    return is_win(value) || is_loss(value); 
}

constexpr Value PawnValue   = 208;
constexpr Value KnightValue = 781;
constexpr Value BishopValue = 825;
constexpr Value RookValue   = 1276;
constexpr Value QueenValue  = 2538;

enum PieceType : uint8_t {
    NO_PIECE_TYPE, 
    PAWN, 
    KNIGHT, 
    BISHOP, 
    ROOK, 
    QUEEN, 
    KING,
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 8
};

enum Piece : uint8_t {
    NO_PIECE,
    W_PAWN = PAWN,     W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN = PAWN + 8, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    PIECE_NB = 16
};

constexpr Value PieceValue[PIECE_NB] = {
    VALUE_ZERO, PawnValue, KnightValue, BishopValue, RookValue, QueenValue, VALUE_ZERO, VALUE_ZERO,
    VALUE_ZERO, PawnValue, KnightValue, BishopValue, RookValue, QueenValue, VALUE_ZERO, VALUE_ZERO
};

using Depth = int;

constexpr Depth DEPTH_QS           = 0;
constexpr Depth DEPTH_UNSEARCHED   = -2;
constexpr Depth DEPTH_ENTRY_OFFSET = -3;

enum Square : uint8_t {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 64
};

enum Direction : int8_t {
    NORTH = 8,
    EAST  = 1,
    SOUTH = -NORTH,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

enum File : uint8_t {
    FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB
};

enum Rank : uint8_t {
    RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB
};

// NNUE dynamic change tracking structures
struct DirtyPiece {
    Piece  pc;
    Square from, to;
    Square remove_sq, add_sq;
    Piece  remove_pc, add_pc;
};

struct DirtyThreat {
    static constexpr int PcSqOffset         = 0;
    static constexpr int ThreatenedSqOffset = 8;
    static constexpr int ThreatenedPcOffset = 16;
    static constexpr int PcOffset           = 20;

    DirtyThreat() noexcept = default;
    constexpr explicit DirtyThreat(uint32_t raw) noexcept : data(raw) {}
    constexpr DirtyThreat(Piece pc, Piece threatened_pc, Square pc_sq, Square threatened_sq, bool add) noexcept {
        data = (static_cast<uint32_t>(add) << 31) 
             | (static_cast<uint32_t>(pc) << PcOffset) 
             | (static_cast<uint32_t>(threatened_pc) << ThreatenedPcOffset)
             | (static_cast<uint32_t>(threatened_sq) << ThreatenedSqOffset) 
             | (static_cast<uint32_t>(pc_sq) << PcSqOffset);
    }

    [[nodiscard]] constexpr Piece  pc() const noexcept { return static_cast<Piece>((data >> PcOffset) & 0xF); }
    [[nodiscard]] constexpr Piece  threatened_pc() const noexcept { return static_cast<Piece>((data >> ThreatenedPcOffset) & 0xF); }
    [[nodiscard]] constexpr Square threatened_sq() const noexcept { return static_cast<Square>((data >> ThreatenedSqOffset) & 0xFF); }
    [[nodiscard]] constexpr Square pc_sq() const noexcept { return static_cast<Square>((data >> PcSqOffset) & 0xFF); }
    [[nodiscard]] constexpr bool   add() const noexcept { return (data >> 31) != 0; }
    [[nodiscard]] constexpr uint32_t raw() const noexcept { return data; }

private:
    uint32_t data;
};

#define ENABLE_INCR_OPERATORS_ON(T) \
    constexpr T& operator++(T& d) noexcept { return d = static_cast<T>(static_cast<int>(d) + 1); } \
    constexpr T& operator--(T& d) noexcept { return d = static_cast<T>(static_cast<int>(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

#undef ENABLE_INCR_OPERATORS_ON

[[nodiscard]] constexpr Direction operator+(Direction d1, Direction d2) noexcept { 
    return static_cast<Direction>(static_cast<int>(d1) + static_cast<int>(d2)); 
}

[[nodiscard]] constexpr Direction operator*(int i, Direction d) noexcept { 
    return static_cast<Direction>(i * static_cast<int>(d)); 
}

[[nodiscard]] constexpr Square operator+(Square s, Direction d) noexcept { 
    return static_cast<Square>(static_cast<int>(s) + static_cast<int>(d)); 
}

[[nodiscard]] constexpr Square operator-(Square s, Direction d) noexcept { 
    return static_cast<Square>(static_cast<int>(s) - static_cast<int>(d)); 
}

constexpr Square& operator+=(Square& s, Direction d) noexcept { return s = s + d; }
constexpr Square& operator-=(Square& s, Direction d) noexcept { return s = s - d; }

[[nodiscard]] constexpr Color operator~(Color c) noexcept { 
    return static_cast<Color>(c ^ BLACK); 
}

[[nodiscard]] constexpr Square flip_rank(Square s) noexcept { 
    return static_cast<Square>(static_cast<uint8_t>(s) ^ static_cast<uint8_t>(SQ_A8)); 
}

[[nodiscard]] constexpr Square flip_file(Square s) noexcept { 
    return static_cast<Square>(static_cast<uint8_t>(s) ^ static_cast<uint8_t>(SQ_H1)); 
}

[[nodiscard]] constexpr Piece operator~(Piece pc) noexcept { 
    return static_cast<Piece>(pc ^ 8); 
}

[[nodiscard]] constexpr CastlingRights operator&(Color c, CastlingRights cr) noexcept {
    return static_cast<CastlingRights>((c == WHITE ? WHITE_CASTLING : BLACK_CASTLING) & cr);
}

[[nodiscard]] constexpr Value mate_in(int ply) noexcept { return VALUE_MATE - ply; }
[[nodiscard]] constexpr Value mated_in(int ply) noexcept { return -VALUE_MATE + ply; }

[[nodiscard]] constexpr Square make_square(File f, Rank r) noexcept { 
    return static_cast<Square>((static_cast<int>(r) << 3) + static_cast<int>(f)); 
}

[[nodiscard]] constexpr Piece make_piece(Color c, PieceType pt) noexcept { 
    return static_cast<Piece>((static_cast<int>(c) << 3) + static_cast<int>(pt)); 
}

[[nodiscard]] constexpr PieceType type_of(Piece pc) noexcept { 
    return static_cast<PieceType>(pc & 7); 
}

[[nodiscard]] constexpr Color color_of(Piece pc) noexcept {
    assert(pc != NO_PIECE);
    return static_cast<Color>(pc >> 3);
}

[[nodiscard]] constexpr bool is_ok(Square s) noexcept { 
    return s >= SQ_A1 && s <= SQ_H8; 
}

[[nodiscard]] constexpr File file_of(Square s) noexcept { 
    return static_cast<File>(static_cast<uint8_t>(s) & 7); 
}

[[nodiscard]] constexpr Rank rank_of(Square s) noexcept { 
    return static_cast<Rank>(s >> 3); 
}

[[nodiscard]] constexpr Square relative_square(Color c, Square s) noexcept { 
    return static_cast<Square>(static_cast<uint8_t>(s) ^ static_cast<uint8_t>(c * 56)); 
}

[[nodiscard]] constexpr Rank relative_rank(Color c, Rank r) noexcept { 
    return static_cast<Rank>(r ^ (c * 7)); 
}

[[nodiscard]] constexpr Rank relative_rank(Color c, Square s) noexcept { 
    return relative_rank(c, rank_of(s)); 
}

[[nodiscard]] constexpr Direction pawn_push(Color c) noexcept { 
    return c == WHITE ? NORTH : SOUTH; 
}

[[nodiscard]] constexpr Key make_key(uint64_t seed) noexcept {
    return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

enum MoveType : uint16_t {
    NORMAL,
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
};

class Move {
public:
    Move() noexcept = default;
    constexpr explicit Move(uint16_t d) noexcept : data(d) {}
    constexpr Move(Square from, Square to) noexcept : data((from << 6) + to) {}

    template<MoveType T>
    [[nodiscard]] static constexpr Move make(Square from, Square to, PieceType pt = KNIGHT) noexcept {
        return Move(T + ((pt - KNIGHT) << 12) + (from << 6) + to);
    }

    [[nodiscard]] constexpr Square from_sq() const noexcept {
        assert(is_ok());
        return static_cast<Square>((data >> 6) & 0x3F);
    }

    [[nodiscard]] constexpr Square to_sq() const noexcept {
        assert(is_ok());
        return static_cast<Square>(data & 0x3F);
    }

    [[nodiscard]] constexpr MoveType type_of() const noexcept { 
        return static_cast<MoveType>(data & (3 << 14)); 
    }

    [[nodiscard]] constexpr PieceType promotion_type() const noexcept { 
        return static_cast<PieceType>(((data >> 12) & 3) + KNIGHT); 
    }

    [[nodiscard]] constexpr bool is_ok() const noexcept { 
        return none().data != data && null().data != data; 
    }

    [[nodiscard]] static constexpr Move null() noexcept { return Move(65); }
    [[nodiscard]] static constexpr Move none() noexcept { return Move(0); }

    [[nodiscard]] constexpr bool operator==(const Move& m) const noexcept = default;
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return data != 0; }
    [[nodiscard]] constexpr uint16_t raw() const noexcept { return data; }

    struct Hash {
        [[nodiscard]] std::size_t operator()(const Move& m) const noexcept { 
            return static_cast<std::size_t>(make_key(m.raw())); 
        }
    };

protected:
    uint16_t data{0};
};

template<typename T, typename... Ts>
struct is_all_same {
    static constexpr bool value = (std::is_same_v<T, Ts> && ...);
};

// helper to verify at compile-time that multiple types in a parameter pack are identical
template<typename... Ts>
constexpr bool is_all_same_v = is_all_same<Ts...>::value;

} // namespace Engine::Core