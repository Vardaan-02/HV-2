#pragma once

#include <cassert>
#include <cstdint>

#include "core/types.h"
#include "movegen/movegen.h"
#include "position/position.h"
#include "search/history.h"

namespace Engine::MoveGen {

using Core::Color;
using Core::Depth;
using Core::ExtMove;
using Core::Move;
using Core::Piece;
using Core::PieceType;
using Core::Square;
using Core::Value;
using Position = Engine::Position::Position;
using Search::ButterflyHistory;
using Search::CounterMoveTable;
using Search::KillerTable;

enum Stage : uint8_t {
    STAGE_MAIN_TT,
    STAGE_CAPTURE_INIT,
    STAGE_GOOD_CAPTURES,
    STAGE_KILLER_1,
    STAGE_KILLER_2,
    STAGE_COUNTERMOVE,
    STAGE_QUIET_INIT,
    STAGE_QUIET_MOVES,
    STAGE_BAD_CAPTURES,

    STAGE_EVASION_TT,
    STAGE_EVASIONS_INIT,
    STAGE_EVASIONS,

    STAGE_QSEARCH_TT,
    STAGE_QCAPTURE_INIT,
    STAGE_QCAPTURES,

    STAGE_DONE
};

class MovePicker {
public:
    MovePicker(const Position& pos,
               Move ttMove,
               Depth depth,
               const ButterflyHistory* history,
               const CounterMoveTable* counterMoves,
               const KillerTable* killers,
               int ply) noexcept;

    MovePicker(const Position& pos,
               Move ttMove,
               const ButterflyHistory* history) noexcept;

    Move next_move() noexcept;

private:
    void score_captures() noexcept;
    void score_quiets() noexcept;
    void score_evasions() noexcept;
    Move select_best(ExtMove* begin, ExtMove* end) noexcept;

    const Position& pos_;
    const ButterflyHistory* history_{nullptr};
    const CounterMoveTable* counterMoves_{nullptr};
    const KillerTable* killers_{nullptr};

    Move ttMove_{Move::none()};
    Move killer1_{Move::none()};
    Move killer2_{Move::none()};
    Move counterMove_{Move::none()};

    Stage stage_{STAGE_DONE};
    Depth depth_{0};
    int ply_{0};

    ExtMove moves_[Core::MAX_MOVES];
    ExtMove* cur_{moves_};
    ExtMove* endMoves_{moves_};
    ExtMove* badCaptures_{moves_ + Core::MAX_MOVES - 1};
};

} // namespace Engine::MoveGen