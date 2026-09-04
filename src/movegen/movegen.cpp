#include "movegen/movegen.h"

#include <cassert>
#include "core/bitboard.h"
#include "core/types.h"

namespace Engine::MoveGen {

using namespace Core;

namespace {

template<Color Us, GenType Type, typename MovePtr>
MovePtr generate_pawn_moves(const Position& pos, MovePtr moveList, Bitboard target) noexcept {
    constexpr Direction Up     = pawn_push(Us);
    constexpr Direction UpLeft = (Us == WHITE) ? NORTH_WEST : SOUTH_WEST;
    constexpr Direction UpRight= (Us == WHITE) ? NORTH_EAST : SOUTH_EAST;
    constexpr Bitboard  Rank7  = (Us == WHITE) ? Rank7BB : Rank2BB;
    constexpr Bitboard  Rank3  = (Us == WHITE) ? Rank3BB : Rank6BB;
    constexpr Bitboard  Rank8  = (Us == WHITE) ? Rank8BB : Rank1BB;

    const Bitboard pawns       = pos.pieces(Us, PAWN);
    const Bitboard emptySquare = ~pos.pieces();
    // Pawn captures MUST target enemy pieces (or evasion target if in check)
    const Bitboard enemies     = (Type == EVASIONS) ? (pos.pieces(~Us) & target) : pos.pieces(~Us);

    // 1. Single and Double Pushes
    if constexpr (Type != CAPTURES) {
        Bitboard b1 = shift<Up>(pawns & ~Rank7) & emptySquare;
        Bitboard b2 = shift<Up>(b1 & Rank3) & emptySquare;

        if constexpr (Type == EVASIONS) {
            b1 &= target;
            b2 &= target;
        }

        Bitboard singlePushes = b1;
        while (singlePushes) {
            const Square to = pop_lsb(singlePushes);
            *moveList++ = Move(to - Up, to);
        }

        while (b2) {
            const Square to = pop_lsb(b2);
            *moveList++ = Move(to - Up - Up, to);
        }
    }

    // 2. Promotions (Pushes onto back rank)
    Bitboard promotions = shift<Up>(pawns & Rank7) & emptySquare;
    if constexpr (Type == EVASIONS) {
        promotions &= target;
    }

    while (promotions) {
        const Square to   = pop_lsb(promotions);
        const Square from = to - Up;
        *moveList++ = Move::make<PROMOTION>(from, to, QUEEN);
        if constexpr (Type != CAPTURES) {
            *moveList++ = Move::make<PROMOTION>(from, to, ROOK);
            *moveList++ = Move::make<PROMOTION>(from, to, BISHOP);
            *moveList++ = Move::make<PROMOTION>(from, to, KNIGHT);
        }
    }

    // 3. Pawn Captures (Diagonal moves MUST intersect enemy pieces)
    Bitboard leftAttacks  = shift<UpLeft>(pawns) & enemies;
    Bitboard rightAttacks = shift<UpRight>(pawns) & enemies;

    Bitboard leftPromotions  = leftAttacks & Rank8;
    Bitboard rightPromotions = rightAttacks & Rank8;
    leftAttacks  &= ~Rank8;
    rightAttacks &= ~Rank8;

    while (leftAttacks) {
        const Square to = pop_lsb(leftAttacks);
        *moveList++ = Move(to - UpLeft, to);
    }

    while (rightAttacks) {
        const Square to = pop_lsb(rightAttacks);
        *moveList++ = Move(to - UpRight, to);
    }

    while (leftPromotions) {
        const Square to   = pop_lsb(leftPromotions);
        const Square from = to - UpLeft;
        *moveList++ = Move::make<PROMOTION>(from, to, QUEEN);
        if constexpr (Type != CAPTURES) {
            *moveList++ = Move::make<PROMOTION>(from, to, ROOK);
            *moveList++ = Move::make<PROMOTION>(from, to, BISHOP);
            *moveList++ = Move::make<PROMOTION>(from, to, KNIGHT);
        }
    }

    while (rightPromotions) {
        const Square to   = pop_lsb(rightPromotions);
        const Square from = to - UpRight;
        *moveList++ = Move::make<PROMOTION>(from, to, QUEEN);
        if constexpr (Type != CAPTURES) {
            *moveList++ = Move::make<PROMOTION>(from, to, ROOK);
            *moveList++ = Move::make<PROMOTION>(from, to, BISHOP);
            *moveList++ = Move::make<PROMOTION>(from, to, KNIGHT);
        }
    }

    // 4. En Passant
    if (pos.ep_square() != SQ_NONE) {
        if constexpr (Type == EVASIONS) {
            if (!(target & square_bb(pos.ep_square() + pawn_push(~Us)))) {
                return moveList;
            }
        }

        Bitboard epPawns = pawns & attacks_bb<PAWN>(pos.ep_square(), ~Us);
        while (epPawns) {
            *moveList++ = Move::make<EN_PASSANT>(pop_lsb(epPawns), pos.ep_square());
        }
    }

    return moveList;
}

template<Color Us, PieceType Pt, typename MovePtr>
MovePtr generate_piece_moves(const Position& pos, MovePtr moveList, Bitboard target) noexcept {
    Bitboard pieces = pos.pieces(Us, Pt);
    while (pieces) {
        const Square from = pop_lsb(pieces);
        Bitboard attacks  = attacks_bb<Pt>(from, pos.pieces()) & target;
        while (attacks) {
            *moveList++ = Move(from, pop_lsb(attacks));
        }
    }
    return moveList;
}

template<Color Us, GenType Type, typename MovePtr>
MovePtr generate_all(const Position& pos, MovePtr moveList, Bitboard target) noexcept {
    moveList = generate_pawn_moves<Us, Type>(pos, moveList, target);
    moveList = generate_piece_moves<Us, KNIGHT>(pos, moveList, target);
    moveList = generate_piece_moves<Us, BISHOP>(pos, moveList, target);
    moveList = generate_piece_moves<Us, ROOK>(pos, moveList, target);
    moveList = generate_piece_moves<Us, QUEEN>(pos, moveList, target);

    if constexpr (Type != EVASIONS) {
        const Square ksq = pos.square<KING>(Us);
        Bitboard kingAttacks = attacks_bb<KING>(ksq) & target;
        while (kingAttacks) {
            *moveList++ = Move(ksq, pop_lsb(kingAttacks));
        }
    }

    if constexpr (Type != CAPTURES && Type != EVASIONS) {
        for (const CastlingRights cr : { static_cast<CastlingRights>(Us & KING_SIDE), 
                                         static_cast<CastlingRights>(Us & QUEEN_SIDE) }) {
            if (!pos.can_castle(cr) || pos.castling_impeded(cr))
                continue;

            *moveList++ = Move::make<CASTLING>(pos.square<KING>(Us), pos.castling_rook_square(cr));
        }
    }

    return moveList;
}

template<Color Us, typename MovePtr>
MovePtr generate_evasions(const Position& pos, MovePtr moveList) noexcept {
    const Square ksq = pos.square<KING>(Us);
    Bitboard kingTargets = attacks_bb<KING>(ksq) & ~pos.pieces(Us);

    while (kingTargets) {
        const Square to = pop_lsb(kingTargets);
        if (!pos.attackers_to_exist(to, pos.pieces() ^ ksq, ~Us)) {
            *moveList++ = Move(ksq, to);
        }
    }

    const Bitboard checkers = pos.checkers();
    if (more_than_one(checkers)) {
        return moveList;
    }

    const Square checkerSq = lsb(checkers);
    const Bitboard target  = between_bb(ksq, checkerSq) | checkerSq;

    return generate_all<Us, EVASIONS>(pos, moveList, target);
}

} // namespace

template<GenType Type, typename MovePtr>
MovePtr generate_dispatch(const Position& pos, MovePtr moveList) noexcept {
    const Color us = pos.side_to_move();

    if constexpr (Type == EVASIONS) {
        return (us == WHITE) ? generate_evasions<WHITE>(pos, moveList)
                             : generate_evasions<BLACK>(pos, moveList);
    } else if constexpr (Type == CAPTURES) {
        const Bitboard target = pos.pieces(~us);
        return (us == WHITE) ? generate_all<WHITE, CAPTURES>(pos, moveList, target)
                             : generate_all<BLACK, CAPTURES>(pos, moveList, target);
    } else if constexpr (Type == QUIETS) {
        const Bitboard target = ~pos.pieces();
        return (us == WHITE) ? generate_all<WHITE, QUIETS>(pos, moveList, target)
                             : generate_all<BLACK, QUIETS>(pos, moveList, target);
    } else if constexpr (Type == NON_EVASIONS) {
        const Bitboard target = ~pos.pieces(us);
        return (us == WHITE) ? generate_all<WHITE, NON_EVASIONS>(pos, moveList, target)
                             : generate_all<BLACK, NON_EVASIONS>(pos, moveList, target);
    } else if constexpr (Type == LEGAL) {
        std::decay_t<decltype(*moveList)> pseudoMoves[MAX_MOVES];
        auto* end = pos.checkers() ? generate_dispatch<EVASIONS>(pos, pseudoMoves)
                                   : generate_dispatch<NON_EVASIONS>(pos, pseudoMoves);
        for (auto* cur = pseudoMoves; cur != end; ++cur) {
            if (pos.legal(*cur)) {
                *moveList++ = *cur;
            }
        }
        return moveList;
    }
}

template<GenType Type>
Move* generate(const Position& pos, Move* moveList) noexcept {
    return generate_dispatch<Type>(pos, moveList);
}

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) noexcept {
    return generate_dispatch<Type>(pos, moveList);
}

template Move* generate<CAPTURES>(const Position&, Move*) noexcept;
template Move* generate<QUIETS>(const Position&, Move*) noexcept;
template Move* generate<EVASIONS>(const Position&, Move*) noexcept;
template Move* generate<NON_EVASIONS>(const Position&, Move*) noexcept;
template Move* generate<LEGAL>(const Position&, Move*) noexcept;

template ExtMove* generate<CAPTURES>(const Position&, ExtMove*) noexcept;
template ExtMove* generate<QUIETS>(const Position&, ExtMove*) noexcept;
template ExtMove* generate<EVASIONS>(const Position&, ExtMove*) noexcept;
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*) noexcept;
template ExtMove* generate<LEGAL>(const Position&, ExtMove*) noexcept;

} // namespace Engine::MoveGen