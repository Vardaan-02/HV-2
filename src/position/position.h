#pragma once

#include <array>
#include <cassert>
#include <deque>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

#include "core/bitboard.h"
#include "core/types.h"

namespace Engine::Position {

using namespace Core;

class TranspositionTable;

// StateInfo stores incremental data required to restore the Position on unmake move.
// A new StateInfo instance is passed on the stack for every do_move() call.
struct StateInfo {
    // Incremental sub-keys and material state (copied during do_move)
    Key    materialKey{0};
    Key    pawnKey{0};
    Key    minorPieceKey{0};
    Key    nonPawnKey[COLOR_NB]{0, 0};
    Value  nonPawnMaterial[COLOR_NB]{0, 0};
    int    castlingRights{NO_CASTLING};
    int    rule50{0};
    int    pliesFromNull{0};
    Square epSquare{SQ_NONE};

    // Recomputed dynamically / lazily evaluated per ply
    Key        key{0};
    Bitboard   checkersBB{0};
    StateInfo* previous{nullptr};
    Bitboard   blockersForKing[COLOR_NB]{0, 0};
    Bitboard   pinners[COLOR_NB]{0, 0};
    Bitboard   checkSquares[PIECE_TYPE_NB]{0, 0, 0, 0, 0, 0, 0, 0};
    Piece      capturedPiece{NO_PIECE};
    int        repetition{0};
};

using StateListPtr = std::unique_ptr<std::deque<StateInfo>>;

class Position {
public:
    static void init() noexcept;

    Position() noexcept = default;
    Position(const Position&) = delete;
    Position& operator=(const Position&) = delete;
    Position(Position&&) noexcept = default;
    Position& operator=(Position&&) noexcept = default;

    // FEN initialization and export
    Position& set(std::string_view fenStr, bool isChess960, StateInfo* si);
    [[nodiscard]] std::string fen() const;

    // Board occupancy accessors
    [[nodiscard]] Bitboard pieces() const noexcept;
    template<typename... PieceTypes>
    [[nodiscard]] Bitboard pieces(PieceTypes... pts) const noexcept;
    [[nodiscard]] Bitboard pieces(Color c) const noexcept;
    template<typename... PieceTypes>
    [[nodiscard]] Bitboard pieces(Color c, PieceTypes... pts) const noexcept;

    [[nodiscard]] Piece piece_on(Square s) const noexcept;
    [[nodiscard]] const std::array<Piece, SQUARE_NB>& piece_array() const noexcept;
    [[nodiscard]] Square ep_square() const noexcept;
    [[nodiscard]] bool empty(Square s) const noexcept;

    template<PieceType Pt>
    [[nodiscard]] int count(Color c) const noexcept;
    template<PieceType Pt>
    [[nodiscard]] int count() const noexcept;
    template<PieceType Pt>
    [[nodiscard]] Square square(Color c) const noexcept;

    // Castling mechanics
    [[nodiscard]] bool can_castle(CastlingRights cr) const noexcept;
    [[nodiscard]] bool castling_impeded(CastlingRights cr) const noexcept;
    [[nodiscard]] Square castling_rook_square(CastlingRights cr) const noexcept;

    // Checks and pinners
    [[nodiscard]] Bitboard checkers() const noexcept;
    [[nodiscard]] Bitboard blockers_for_king(Color c) const noexcept;
    [[nodiscard]] Bitboard check_squares(PieceType pt) const noexcept;
    [[nodiscard]] Bitboard pinners(Color c) const noexcept;

    // Attack ray projections
    [[nodiscard]] Bitboard attackers_to(Square s) const noexcept;
    [[nodiscard]] Bitboard attackers_to(Square s, Bitboard occupied) const noexcept;
    [[nodiscard]] bool attackers_to_exist(Square s, Bitboard occupied, Color c) const noexcept;
    void update_slider_blockers(Color c) const noexcept;
    template<PieceType Pt>
    [[nodiscard]] Bitboard attacks_by(Color c) const noexcept;

    // Move classification and legality
    [[nodiscard]] bool legal(Move m) const noexcept;
    [[nodiscard]] bool pseudo_legal(Move m) const noexcept;
    [[nodiscard]] bool capture(Move m) const noexcept;
    [[nodiscard]] bool capture_stage(Move m) const noexcept;
    [[nodiscard]] bool gives_check(Move m) const noexcept;
    [[nodiscard]] Piece moved_piece(Move m) const noexcept;
    [[nodiscard]] Piece captured_piece() const noexcept;

    // Move mutation (Search Tree DFS)
    void do_move(Move m, StateInfo& newSt, bool givesCheck) noexcept;
    void do_move(Move m, StateInfo& newSt) noexcept;
    void undo_move(Move m) noexcept;
    void do_null_move(StateInfo& newSt) noexcept;
    void undo_null_move() noexcept;

    // Static Exchange Evaluation (SEE)
    [[nodiscard]] bool see_ge(Move m, int threshold = 0) const noexcept;

    // Hash keys
    [[nodiscard]] Key key() const noexcept;
    [[nodiscard]] Key material_key() const noexcept;
    [[nodiscard]] Key pawn_key() const noexcept;
    [[nodiscard]] Key minor_piece_key() const noexcept;
    [[nodiscard]] Key non_pawn_key(Color c) const noexcept;

    // Game state diagnostics
    [[nodiscard]] Color side_to_move() const noexcept;
    [[nodiscard]] int game_ply() const noexcept;
    [[nodiscard]] bool is_chess960() const noexcept;
    [[nodiscard]] bool is_draw(int ply) const noexcept;
    [[nodiscard]] bool is_repetition(int ply) const noexcept;
    [[nodiscard]] bool upcoming_repetition(int ply) const noexcept;
    [[nodiscard]] bool has_repeated() const noexcept;
    [[nodiscard]] int rule50_count() const noexcept;
    [[nodiscard]] Value non_pawn_material(Color c) const noexcept;
    [[nodiscard]] Value non_pawn_material() const noexcept;

    // Debugging and verification
    [[nodiscard]] bool pos_is_ok() const noexcept;
    [[nodiscard]] bool material_key_is_ok() const noexcept;
    void flip() noexcept;

    [[nodiscard]] StateInfo* state() const noexcept;

    void put_piece(Piece pc, Square s) noexcept;
    void remove_piece(Square s) noexcept;
    void swap_piece(Square s, Piece pc) noexcept;

private:
    void set_castling_right(Color c, Square rfrom) noexcept;
    [[nodiscard]] Key compute_material_key() const noexcept;
    void set_state() const noexcept;
    void set_check_info() const noexcept;

    void move_piece(Square from, Square to) noexcept;
    template<bool Do>
    void do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto) noexcept;
    [[nodiscard]] Key adjust_key50(Key k) const noexcept;

    // Board representation data
    std::array<Piece, SQUARE_NB>        board{};
    std::array<Bitboard, PIECE_TYPE_NB> byTypeBB{};
    std::array<Bitboard, COLOR_NB>      byColorBB{};

    int        pieceCount[PIECE_NB]{};
    int        castlingRightsMask[SQUARE_NB]{};
    Square     castlingRookSquare[CASTLING_RIGHT_NB]{};
    Bitboard   castlingPath[CASTLING_RIGHT_NB]{};
    StateInfo* st{nullptr};
    int        gamePly{0};
    Color      sideToMove{WHITE};
    bool       chess960{false};
};

std::ostream& operator<<(std::ostream& os, const Position& pos);

// Inline Definitions

inline Color Position::side_to_move() const noexcept { return sideToMove; }

inline Piece Position::piece_on(Square s) const noexcept {
    assert(is_ok(s));
    return board[s];
}

inline const std::array<Piece, SQUARE_NB>& Position::piece_array() const noexcept { return board; }

inline bool Position::empty(Square s) const noexcept { return piece_on(s) == NO_PIECE; }

inline Piece Position::moved_piece(Move m) const noexcept { return piece_on(m.from_sq()); }

inline Bitboard Position::pieces() const noexcept { return byTypeBB[ALL_PIECES]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(PieceTypes... pts) const noexcept {
    return (byTypeBB[pts] | ...);
}

inline Bitboard Position::pieces(Color c) const noexcept { return byColorBB[c]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(Color c, PieceTypes... pts) const noexcept {
    return pieces(c) & pieces(pts...);
}

template<PieceType Pt>
inline int Position::count(Color c) const noexcept {
    return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt>
inline int Position::count() const noexcept {
    return count<Pt>(WHITE) + count<Pt>(BLACK);
}

template<PieceType Pt>
inline Square Position::square(Color c) const noexcept {
    assert(count<Pt>(c) == 1);
    return lsb(pieces(c, Pt));
}

inline Square Position::ep_square() const noexcept { return st->epSquare; }

inline bool Position::can_castle(CastlingRights cr) const noexcept { return (st->castlingRights & cr) != 0; }

inline bool Position::castling_impeded(CastlingRights cr) const noexcept {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return (pieces() & castlingPath[cr]) != 0;
}

inline Square Position::castling_rook_square(CastlingRights cr) const noexcept {
    assert(cr == WHITE_OO || cr == WHITE_OOO || cr == BLACK_OO || cr == BLACK_OOO);
    return castlingRookSquare[cr];
}

inline Bitboard Position::attackers_to(Square s) const noexcept { return attackers_to(s, pieces()); }

template<PieceType Pt>
inline Bitboard Position::attacks_by(Color c) const noexcept {
    if constexpr (Pt == PAWN) {
        return (c == WHITE) ? pawn_attacks_bb<WHITE>(pieces(WHITE, PAWN))
                            : pawn_attacks_bb<BLACK>(pieces(BLACK, PAWN));
    } else {
        Bitboard threats = 0;
        Bitboard attackers = pieces(c, Pt);
        while (attackers) {
            threats |= attacks_bb<Pt>(pop_lsb(attackers), pieces());
        }
        return threats;
    }
}

inline Bitboard Position::checkers() const noexcept { return st->checkersBB; }

inline Bitboard Position::blockers_for_king(Color c) const noexcept { return st->blockersForKing[c]; }

inline Bitboard Position::pinners(Color c) const noexcept { return st->pinners[c]; }

inline Bitboard Position::check_squares(PieceType pt) const noexcept { return st->checkSquares[pt]; }

inline Key Position::key() const noexcept { return adjust_key50(st->key); }

inline Key Position::adjust_key50(Key k) const noexcept {
    return (st->rule50 < 14) ? k : (k ^ make_key((st->rule50 - 14) / 8));
}

inline Key Position::pawn_key() const noexcept { return st->pawnKey; }

inline Key Position::material_key() const noexcept { return st->materialKey; }

inline Key Position::minor_piece_key() const noexcept { return st->minorPieceKey; }

inline Key Position::non_pawn_key(Color c) const noexcept { return st->nonPawnKey[c]; }

inline Value Position::non_pawn_material(Color c) const noexcept { return st->nonPawnMaterial[c]; }

inline Value Position::non_pawn_material() const noexcept {
    return non_pawn_material(WHITE) + non_pawn_material(BLACK);
}

inline int Position::game_ply() const noexcept { return gamePly; }

inline int Position::rule50_count() const noexcept { return st->rule50; }

inline bool Position::is_chess960() const noexcept { return chess960; }

inline bool Position::capture(Move m) const noexcept {
    assert(m.is_ok());
    return (!empty(m.to_sq()) && m.type_of() != CASTLING) || (m.type_of() == EN_PASSANT);
}

inline bool Position::capture_stage(Move m) const noexcept {
    assert(m.is_ok());
    return capture(m) || (m.promotion_type() == QUEEN);
}

inline Piece Position::captured_piece() const noexcept { return st->capturedPiece; }

inline void Position::put_piece(Piece pc, Square s) noexcept {
    board[s] = pc;
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
    byColorBB[color_of(pc)] |= s;
    pieceCount[pc]++;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
}

inline void Position::remove_piece(Square s) noexcept {
    const Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
    pieceCount[pc]--;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
}

inline void Position::move_piece(Square from, Square to) noexcept {
    const Piece pc = board[from];
    const Bitboard fromTo = square_bb(from) | square_bb(to);

    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to]   = pc;
}

inline void Position::swap_piece(Square s, Piece pc) noexcept {
    remove_piece(s);
    put_piece(pc, s);
}

inline void Position::do_move(Move m, StateInfo& newSt) noexcept {
    do_move(m, newSt, gives_check(m));
}

inline StateInfo* Position::state() const noexcept { return st; }

} // namespace Engine::Position