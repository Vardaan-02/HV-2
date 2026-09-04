#include "position/position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

#include "core/intrinsics.h"

namespace Engine::Position {

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key enpassant[FILE_NB];
Key castling[CASTLING_RIGHT_NB];
Key side;
Key noPawns;

} // namespace Zobrist

namespace {

constexpr std::string_view PieceToChar(" PNBRQK  pnbrqk");

constexpr Piece AllPieces[] = {
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING
};

// Fast 64-bit SplitMix PRNG for Zobrist key initialization
class SimplePRNG {
public:
    constexpr explicit SimplePRNG(uint64_t seed) noexcept : state(seed) {}

    constexpr uint64_t rand64() noexcept {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

private:
    uint64_t state;
};

// Cuckoo hash table for fast repetition detection
inline int H1(Key h) noexcept { return static_cast<int>(h & 0x1FFF); }
inline int H2(Key h) noexcept { return static_cast<int>((h >> 16) & 0x1FFF); }

std::array<Key, 8192>  cuckoo;
std::array<Move, 8192> cuckooMove;

} // namespace

void Position::init() noexcept {
    SimplePRNG rng(1070372ULL);

    for (Piece pc : AllPieces) {
        for (Square s = SQ_A1; s <= SQ_H8; ++s) {
            Zobrist::psq[pc][s] = rng.rand64();
        }
    }

    // Pawns cannot exist on rank 1 and 8
    std::fill_n(Zobrist::psq[W_PAWN] + SQ_A8, 8, 0ULL);
    std::fill_n(Zobrist::psq[B_PAWN], 8, 0ULL);

    for (File f = FILE_A; f <= FILE_H; ++f) {
        Zobrist::enpassant[f] = rng.rand64();
    }

    for (int cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr) {
        Zobrist::castling[cr] = rng.rand64();
    }

    Zobrist::side    = rng.rand64();
    Zobrist::noPawns = rng.rand64();

    // Populate Cuckoo repetition tables with reversible non-pawn moves
    cuckoo.fill(0);
    cuckooMove.fill(Move::none());

    for (Piece pc : AllPieces) {
        for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
            for (Square s2 = static_cast<Square>(s1 + 1); s2 <= SQ_H8; ++s2) {
                if (type_of(pc) != PAWN && (attacks_bb(type_of(pc), s1, 0) & s2)) {
                    Move move = Move(s1, s2);
                    Key  key  = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                    int  i    = H1(key);

                    while (true) {
                        std::swap(cuckoo[i], key);
                        std::swap(cuckooMove[i], move);
                        if (move == Move::none())
                            break;
                        i = (i == H1(key)) ? H2(key) : H1(key);
                    }
                }
            }
        }
    }
}

Position& Position::set(std::string_view fenStr, bool isChess960, StateInfo* si) {
    Square sq = SQ_A8;
    std::istringstream ss{std::string(fenStr)};

    std::memset(reinterpret_cast<void*>(this), 0, sizeof(Position));
    std::memset(reinterpret_cast<void*>(si), 0, sizeof(StateInfo));
    st = si;

    ss >> std::noskipws;
    unsigned char token;

    // 1. Piece placement
    while ((ss >> token) && !std::isspace(token)) {
        if (std::isdigit(token)) {
            sq += (token - '0') * EAST;
        } else if (token == '/') {
            sq += 2 * SOUTH;
        } else {
            const size_t idx = PieceToChar.find(token);
            if (idx != std::string_view::npos) {
                put_piece(static_cast<Piece>(idx), sq);
                ++sq;
            }
        }
    }

    // 2. Active side to move
    ss >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;

    // 3. Castling availability
    while ((ss >> token) && !std::isspace(token)) {
        Square rsq;
        const Color c = std::islower(token) ? BLACK : WHITE;
        const Piece rook = make_piece(c, ROOK);

        token = static_cast<unsigned char>(std::toupper(token));

        if (token == 'K') {
            for (rsq = relative_square(c, SQ_H1); piece_on(rsq) != rook; --rsq) {}
        } else if (token == 'Q') {
            for (rsq = relative_square(c, SQ_A1); piece_on(rsq) != rook; ++rsq) {}
        } else if (token >= 'A' && token <= 'H') {
            rsq = make_square(static_cast<File>(token - 'A'), relative_rank(c, RANK_1));
        } else {
            continue;
        }

        set_castling_right(c, rsq);
    }

    // 4. En passant target square
    unsigned char col, row;
    bool enpassant = false;

    if (((ss >> col) && (col >= 'a' && col <= 'h'))
        && ((ss >> row) && (row == (sideToMove == WHITE ? '6' : '3')))) {
        st->epSquare = make_square(static_cast<File>(col - 'a'), static_cast<Rank>(row - '1'));

        enpassant = (attacks_bb<PAWN>(st->epSquare, ~sideToMove) & pieces(sideToMove, PAWN))
                 && (pieces(~sideToMove, PAWN) & (st->epSquare + pawn_push(~sideToMove)))
                 && !(pieces() & (st->epSquare | (st->epSquare + pawn_push(sideToMove))));
    }

    if (!enpassant)
        st->epSquare = SQ_NONE;

    // 5 & 6. Halfmove clock and fullmove number
    ss >> std::skipws >> st->rule50 >> gamePly;
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    chess960 = isChess960;
    set_state();

    assert(pos_is_ok());
    return *this;
}

void Position::set_castling_right(Color c, Square rfrom) noexcept {
    const Square kfrom = square<KING>(c);
    const CastlingRights cr = static_cast<CastlingRights>(c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE));

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    const Square kto = relative_square(c, (cr & KING_SIDE) ? SQ_G1 : SQ_C1);
    const Square rto = relative_square(c, (cr & KING_SIDE) ? SQ_F1 : SQ_D1);

    castlingPath[cr] = (between_bb(rfrom, rto) | between_bb(kfrom, kto)) & ~(kfrom | rfrom);
}

void Position::set_check_info() const noexcept {
    update_slider_blockers(WHITE);
    update_slider_blockers(BLACK);

    const Square ksq = square<KING>(~sideToMove);

    st->checkSquares[PAWN]   = attacks_bb<PAWN>(ksq, ~sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb<KNIGHT>(ksq);
    st->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
    st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;
}

void Position::set_state() const noexcept {
    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey           = Zobrist::noPawns;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB        = attackers_to(square<KING>(sideToMove)) & pieces(~sideToMove);

    set_check_info();

    for (Bitboard b = pieces(); b;) {
        const Square s  = pop_lsb(b);
        const Piece  pc = piece_on(s);
        st->key ^= Zobrist::psq[pc][s];

        if (type_of(pc) == PAWN) {
            st->pawnKey ^= Zobrist::psq[pc][s];
        } else {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];
            if (type_of(pc) != KING) {
                st->nonPawnMaterial[color_of(pc)] += PieceValue[pc];
                if (type_of(pc) <= BISHOP) {
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
                }
            }
        }
    }

    if (st->epSquare != SQ_NONE)
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;

    st->key ^= Zobrist::castling[st->castlingRights];
    st->materialKey = compute_material_key();
}

Key Position::compute_material_key() const noexcept {
    Key k = 0;
    for (Piece pc : AllPieces) {
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt) {
            k ^= Zobrist::psq[pc][8 + cnt];
        }
    }
    return k;
}

std::string Position::fen() const {
    std::ostringstream ss;

    for (int r = static_cast<int>(RANK_8); r >= static_cast<int>(RANK_1); --r) {
        int emptyCnt = 0;
        for (File f = FILE_A; f <= FILE_H; ++f) {
            const Square s = make_square(f, static_cast<Rank>(r));
            if (empty(s)) {
                ++emptyCnt;
            } else {
                if (emptyCnt > 0) {
                    ss << emptyCnt;
                    emptyCnt = 0;
                }
                ss << PieceToChar[piece_on(s)];
            }
        }
        if (emptyCnt > 0) ss << emptyCnt;
        if (r > static_cast<int>(RANK_1)) ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    if (can_castle(WHITE_OO))
        ss << (chess960 ? static_cast<char>('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');
    if (can_castle(WHITE_OOO))
        ss << (chess960 ? static_cast<char>('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');
    if (can_castle(BLACK_OO))
        ss << (chess960 ? static_cast<char>('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');
    if (can_castle(BLACK_OOO))
        ss << (chess960 ? static_cast<char>('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');
    if (!can_castle(ANY_CASTLING))
        ss << '-';

    if (ep_square() == SQ_NONE) {
        ss << " - ";
    } else {
        ss << " " << static_cast<char>('a' + file_of(ep_square()))
           << static_cast<char>('1' + rank_of(ep_square())) << " ";
    }

    ss << st->rule50 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;
    return ss.str();
}

void Position::update_slider_blockers(Color c) const noexcept {
    const Square ksq = square<KING>(c);
    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    Bitboard snipers = ((attacks_bb<ROOK>(ksq) & pieces(QUEEN, ROOK))
                     | (attacks_bb<BISHOP>(ksq) & pieces(QUEEN, BISHOP)))
                     & pieces(~c);
    const Bitboard occupancy = pieces() ^ snipers;

    while (snipers) {
        const Square   sniperSq = pop_lsb(snipers);
        const Bitboard b        = between_bb(ksq, sniperSq) & occupancy;

        if (b && !more_than_one(b)) {
            st->blockersForKing[c] |= b;
            if (b & pieces(c)) {
                st->pinners[~c] |= sniperSq;
            }
        }
    }
}

Bitboard Position::attackers_to(Square s, Bitboard occupied) const noexcept {
    return (attacks_bb<ROOK>(s, occupied) & pieces(ROOK, QUEEN))
         | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP, QUEEN))
         | (attacks_bb<PAWN>(s, BLACK) & pieces(WHITE, PAWN))
         | (attacks_bb<PAWN>(s, WHITE) & pieces(BLACK, PAWN))
         | (attacks_bb<KNIGHT>(s) & pieces(KNIGHT)) 
         | (attacks_bb<KING>(s) & pieces(KING));
}

bool Position::attackers_to_exist(Square s, Bitboard occupied, Color c) const noexcept {
    return ((attacks_bb<ROOK>(s) & pieces(c, ROOK, QUEEN))
            && (attacks_bb<ROOK>(s, occupied) & pieces(c, ROOK, QUEEN)))
        || ((attacks_bb<BISHOP>(s) & pieces(c, BISHOP, QUEEN))
            && (attacks_bb<BISHOP>(s, occupied) & pieces(c, BISHOP, QUEEN)))
        || (((attacks_bb<PAWN>(s, ~c) & pieces(PAWN)) 
             | (attacks_bb<KNIGHT>(s) & pieces(KNIGHT))
             | (attacks_bb<KING>(s) & pieces(KING))) & pieces(c));
}

bool Position::legal(Move m) const noexcept {
    assert(m.is_ok());

    const Color  us   = sideToMove;
    const Square from = m.from_sq();
    Square       to   = m.to_sq();

    assert(color_of(moved_piece(m)) == us);
    assert(piece_on(square<KING>(us)) == make_piece(us, KING));

    if (m.type_of() == EN_PASSANT) {
        const Square   ksq      = square<KING>(us);
        const Square   capsq    = to - pawn_push(us);
        const Bitboard occupied = (pieces() ^ square_bb(from) ^ square_bb(capsq)) | square_bb(to);

        return !(attacks_bb<ROOK>(ksq, occupied) & pieces(~us, QUEEN, ROOK))
            && !(attacks_bb<BISHOP>(ksq, occupied) & pieces(~us, QUEEN, BISHOP));
    }

    if (m.type_of() == CASTLING) {
        // King cannot castle out of check
        if (checkers())
            return false;

        // In our generator, castling 'to' is the rook's initial square
        const Square kingTo = relative_square(us, to > from ? SQ_G1 : SQ_C1);
        const Direction step = (kingTo > from) ? EAST : WEST;

        // Check transit squares between king's starting square and destination square
        for (Square s = from + step; s != kingTo + step; s += step) {
            if (attackers_to_exist(s, pieces(), ~us))
                return false;
        }

        return !chess960 || !(blockers_for_king(us) & to);
    }

    if (type_of(piece_on(from)) == KING) {
        return !attackers_to_exist(to, pieces() ^ square_bb(from), ~us);
    }

    return !(blockers_for_king(us) & from) || (line_bb(from, to) & pieces(us, KING));
}

bool Position::pseudo_legal(Move m) const noexcept {
    const Color  us   = sideToMove;
    const Square from = m.from_sq();
    const Square to   = m.to_sq();
    const Piece  pc   = moved_piece(m);

    if (pc == NO_PIECE || color_of(pc) != us) return false;
    if (pieces(us) & to) return false;

    if (type_of(pc) == PAWN) {
        if ((Rank8BB | Rank1BB) & to) return false;

        const bool isCapture    = (attacks_bb<PAWN>(from, us) & pieces(~us) & to) != 0;
        const bool isSinglePush = (from + pawn_push(us) == to) && empty(to);
        const bool isDoublePush = (from + 2 * pawn_push(us) == to)
                               && (relative_rank(us, from) == RANK_2) && empty(to)
                               && empty(to - pawn_push(us));

        if (!(isCapture || isSinglePush || isDoublePush)) return false;
    } else if (!(attacks_bb(type_of(pc), from, pieces()) & to)) {
        return false;
    }

    if (checkers()) {
        if (type_of(pc) != KING) {
            if (more_than_one(checkers())) return false;
            if (!(between_bb(square<KING>(us), lsb(checkers())) & to)) return false;
        } else if (attackers_to_exist(to, pieces() ^ from, ~us)) {
            return false;
        }
    }
    return true;
}

bool Position::gives_check(Move m) const noexcept {
    assert(m.is_ok());
    const Square from = m.from_sq();
    const Square to   = m.to_sq();

    if (check_squares(type_of(piece_on(from))) & to) return true;

    if (blockers_for_king(~sideToMove) & from)
        return !(line_bb(from, to) & pieces(~sideToMove, KING)) || m.type_of() == CASTLING;

    switch (m.type_of()) {
        case NORMAL:
            return false;
        case PROMOTION:
            return (attacks_bb(m.promotion_type(), to, pieces() ^ from) & pieces(~sideToMove, KING)) != 0;
        case EN_PASSANT: {
            const Square capsq = make_square(file_of(to), rank_of(from));
            const Bitboard b   = (pieces() ^ from ^ capsq) | to;
            return (attacks_bb<ROOK>(square<KING>(~sideToMove), b) & pieces(sideToMove, QUEEN, ROOK))
                 | (attacks_bb<BISHOP>(square<KING>(~sideToMove), b) & pieces(sideToMove, QUEEN, BISHOP));
        }
        case CASTLING: {
            const Square rto = relative_square(sideToMove, to > from ? SQ_F1 : SQ_D1);
            return (check_squares(ROOK) & rto) != 0;
        }
    }
    return false;
}

void Position::do_move(Move m, StateInfo& newSt, bool givesCheck) noexcept {
    assert(m.is_ok());
    assert(&newSt != st);

    Key k = st->key ^ Zobrist::side;

    newSt.materialKey        = st->materialKey;
    newSt.pawnKey            = st->pawnKey;
    newSt.minorPieceKey      = st->minorPieceKey;
    newSt.nonPawnKey[WHITE]  = st->nonPawnKey[WHITE];
    newSt.nonPawnKey[BLACK]  = st->nonPawnKey[BLACK];
    newSt.nonPawnMaterial[WHITE] = st->nonPawnMaterial[WHITE];
    newSt.nonPawnMaterial[BLACK] = st->nonPawnMaterial[BLACK];
    newSt.castlingRights     = st->castlingRights;
    newSt.rule50             = st->rule50;
    newSt.pliesFromNull      = st->pliesFromNull;
    newSt.epSquare           = st->epSquare;
    newSt.previous = st;
    st             = &newSt;

    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;

    const Color  us       = sideToMove;
    const Color  them     = ~us;
    const Square from     = m.from_sq();
    Square       to       = m.to_sq();
    const Piece  pc       = piece_on(from);
    Piece        captured = (m.type_of() == EN_PASSANT) ? make_piece(them, PAWN) : piece_on(to);

    bool checkEP = false;

    if (m.type_of() == CASTLING) {
        Square rfrom, rto;
        do_castling<true>(us, from, to, rfrom, rto);

        k ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        st->nonPawnKey[us] ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
        captured = NO_PIECE;
    } else if (captured != NO_PIECE) {
        Square capsq = to;

        if (type_of(captured) == PAWN) {
            if (m.type_of() == EN_PASSANT) {
                capsq -= pawn_push(us);
                remove_piece(capsq);
            }
            st->pawnKey ^= Zobrist::psq[captured][capsq];
        } else {
            st->nonPawnMaterial[them] -= PieceValue[captured];
            st->nonPawnKey[them] ^= Zobrist::psq[captured][capsq];
            if (type_of(captured) <= BISHOP) {
                st->minorPieceKey ^= Zobrist::psq[captured][capsq];
            }
        }

        k ^= Zobrist::psq[captured][capsq];
        st->materialKey ^= Zobrist::psq[captured][8 + pieceCount[captured] - (m.type_of() != EN_PASSANT)];
        st->rule50 = 0;
    }

    k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

    if (st->epSquare != SQ_NONE) {
        k ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    if (st->castlingRights && (castlingRightsMask[from] | castlingRightsMask[to])) {
        k ^= Zobrist::castling[st->castlingRights];
        st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
        k ^= Zobrist::castling[st->castlingRights];
    }

    if (m.type_of() != CASTLING) {
        if (captured && m.type_of() != EN_PASSANT) {
            remove_piece(from);
            swap_piece(to, pc);
        } else {
            move_piece(from, to);
        }
    }

    if (type_of(pc) == PAWN) {
        if ((static_cast<int>(to) ^ static_cast<int>(from)) == 16) {
            checkEP = true;
        } else if (m.type_of() == PROMOTION) {
            const Piece promotion = make_piece(us, m.promotion_type());
            swap_piece(to, promotion);

            k ^= Zobrist::psq[promotion][to];
            st->materialKey ^= Zobrist::psq[promotion][8 + pieceCount[promotion] - 1]
                             ^ Zobrist::psq[pc][8 + pieceCount[pc]];
            st->nonPawnKey[us] ^= Zobrist::psq[promotion][to];

            if (type_of(promotion) <= BISHOP)
                st->minorPieceKey ^= Zobrist::psq[promotion][to];

            st->nonPawnMaterial[us] += PieceValue[promotion];
        }

        st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        st->rule50 = 0;
    } else {
        st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
        if (type_of(pc) <= BISHOP)
            st->minorPieceKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }

    st->capturedPiece = captured;
    st->checkersBB    = givesCheck ? attackers_to(square<KING>(them)) & pieces(us) : 0;

    sideToMove = ~sideToMove;
    set_check_info();

    while (checkEP) {
        const Bitboard pawns = attacks_bb<PAWN>(to - pawn_push(us), us) & pieces(them, PAWN);
        if (!pawns || (checkers() & ~square_bb(to))) break;

        st->epSquare = to - pawn_push(us);
        k ^= Zobrist::enpassant[file_of(st->epSquare)];
        break;
    }

    st->key = k;

    // Repetition check traversal
    st->repetition = 0;
    const int end  = std::min(st->rule50, st->pliesFromNull);
    if (end >= 4) {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2) {
            stp = stp->previous->previous;
            if (stp->key == st->key) {
                st->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }

    assert(pos_is_ok());
}

void Position::undo_move(Move m) noexcept {
    assert(m.is_ok());

    sideToMove = ~sideToMove;

    const Color  us   = sideToMove;
    const Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece        pc   = piece_on(to);

    if (m.type_of() == PROMOTION) {
        remove_piece(to);
        pc = make_piece(us, PAWN);
        put_piece(pc, to);
    }

    if (m.type_of() == CASTLING) {
        Square rfrom, rto;
        do_castling<false>(us, from, to, rfrom, rto);
    } else {
        move_piece(to, from);

        if (st->capturedPiece != NO_PIECE) {
            Square capsq = to;
            if (m.type_of() == EN_PASSANT) {
                capsq -= pawn_push(us);
            }
            put_piece(st->capturedPiece, capsq);
        }
    }

    st = st->previous;
    --gamePly;

    assert(pos_is_ok());
}

template<bool Do>
void Position::do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto) noexcept {
    const bool kingSide = to > from;
    rfrom = to;
    rto   = relative_square(us, kingSide ? SQ_F1 : SQ_D1);
    to    = relative_square(us, kingSide ? SQ_G1 : SQ_C1);

    remove_piece(Do ? from : to);
    remove_piece(Do ? rfrom : rto);
    put_piece(make_piece(us, KING), Do ? to : from);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom);
}

void Position::do_null_move(StateInfo& newSt) noexcept {
    assert(!checkers());
    assert(&newSt != st);

    std::memcpy(&newSt, st, sizeof(StateInfo));
    newSt.previous = st;
    st             = &newSt;

    if (st->epSquare != SQ_NONE) {
        st->key ^= Zobrist::enpassant[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    st->key ^= Zobrist::side;
    st->pliesFromNull = 0;

    sideToMove = ~sideToMove;
    set_check_info();
    st->repetition = 0;

    assert(pos_is_ok());
}

void Position::undo_null_move() noexcept {
    assert(!checkers());
    st = st->previous;
    sideToMove = ~sideToMove;
}

bool Position::see_ge(Move m, int threshold) const noexcept {
    assert(m.is_ok());
    if (m.type_of() != NORMAL) return VALUE_ZERO >= threshold;

    const Square from = m.from_sq();
    const Square to   = m.to_sq();

    int swap = PieceValue[piece_on(to)] - threshold;
    if (swap < 0) return false;

    swap = PieceValue[piece_on(from)] - swap;
    if (swap <= 0) return true;

    Bitboard occupied  = pieces() ^ from ^ to;
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int      res = 1;

    while (true) {
        stm = ~stm;
        attackers &= occupied;

        if (!(stmAttackers = attackers & pieces(stm))) break;

        if (pinners(~stm) & occupied) {
            stmAttackers &= ~blockers_for_king(stm);
            if (!stmAttackers) break;
        }

        res ^= 1;

        if ((bb = stmAttackers & pieces(PAWN))) {
            if ((swap = PawnValue - swap) < res) break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(KNIGHT))) {
            if ((swap = KnightValue - swap) < res) break;
            occupied ^= least_significant_square_bb(bb);
        } else if ((bb = stmAttackers & pieces(BISHOP))) {
            if ((swap = BishopValue - swap) < res) break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(ROOK))) {
            if ((swap = RookValue - swap) < res) break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
        } else if ((bb = stmAttackers & pieces(QUEEN))) {
            swap = QueenValue - swap;
            occupied ^= least_significant_square_bb(bb);
            attackers |= (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                       | (attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN));
        } else {
            return (attackers & ~pieces(stm)) ? (res ^ 1) : res;
        }
    }

    return res != 0;
}

bool Position::is_draw(int ply) const noexcept {
    if (st->rule50 > 99 && !checkers()) return true;
    return is_repetition(ply);
}

bool Position::is_repetition(int ply) const noexcept {
    return st->repetition && st->repetition < ply;
}

bool Position::has_repeated() const noexcept {
    StateInfo* stc = st;
    int end = std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4) {
        if (stc->repetition) return true;
        stc = stc->previous;
    }
    return false;
}

bool Position::upcoming_repetition(int ply) const noexcept {
    const int end = std::min(st->rule50, st->pliesFromNull);
    if (end < 3) return false;

    const Key  originalKey = st->key;
    StateInfo* stp         = st->previous;
    Key        other       = originalKey ^ stp->key ^ Zobrist::side;

    for (int i = 3; i <= end; i += 2) {
        stp = stp->previous;
        other ^= stp->key ^ stp->previous->key ^ Zobrist::side;
        stp = stp->previous;

        if (other != 0) continue;

        const Key moveKey = originalKey ^ stp->key;
        int j = H1(moveKey);
        if (cuckoo[j] != moveKey) {
            j = H2(moveKey);
            if (cuckoo[j] != moveKey) continue;
        }

        const Move   move = cuckooMove[j];
        const Square s1   = move.from_sq();
        const Square s2   = move.to_sq();

        if (!((between_bb(s1, s2) ^ s2) & pieces())) {
            if (ply > i || stp->repetition) return true;
        }
    }
    return false;
}

void Position::flip() noexcept {
    std::string f, token;
    std::stringstream ss{fen()};

    for (int r = static_cast<int>(RANK_8); r >= static_cast<int>(RANK_1); --r) {
        std::getline(ss, token, (r > static_cast<int>(RANK_1)) ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));
    }

    ss >> token;
    f += (token == "w" ? "B " : "W ");

    ss >> token;
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) {
        return static_cast<char>(std::islower(c) ? std::toupper(c) : std::tolower(c));
    });

    ss >> token;
    f += (token == "-" ? token : token.replace(1, 1, token[1] == '3' ? "6" : "3"));

    std::getline(ss, token);
    f += token;

    set(f, is_chess960(), st);
    assert(pos_is_ok());
}

bool Position::material_key_is_ok() const noexcept {
    return compute_material_key() == st->materialKey;
}

bool Position::pos_is_ok() const noexcept {
    if (sideToMove != WHITE && sideToMove != BLACK) return false;
    if (piece_on(square<KING>(WHITE)) != W_KING) return false;
    if (piece_on(square<KING>(BLACK)) != B_KING) return false;
    if (ep_square() != SQ_NONE && relative_rank(sideToMove, ep_square()) != RANK_6) return false;
    return true;
}

std::ostream& operator<<(std::ostream& os, const Position& pos) {
    os << "\n +---+---+---+---+---+---+---+---+\n";
    for (int r = static_cast<int>(RANK_8); r >= static_cast<int>(RANK_1); --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
            os << " | " << PieceToChar[pos.piece_on(make_square(f, static_cast<Rank>(r)))];
        }
        os << " | " << (r + 1) << "\n +---+---+---+---+---+---+---+---+\n";
    }
    os << "   a   b   c   d   e   f   g   h\n\n";
    os << "Fen: " << pos.fen() << "\n";
    os << "Key: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16)
       << pos.key() << std::dec << "\n";
    return os;
}

} // namespace Engine::Position