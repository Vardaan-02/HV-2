#include "movegen/movepicker.h"

#include <algorithm>

namespace Engine::MoveGen {

namespace {

constexpr int VICTIM_SCORES[Core::PIECE_TYPE_NB] = {
    0, 100, 300, 300, 500, 900, 0
};

} // namespace

MovePicker::MovePicker(const Position& pos,
                       Move ttMove,
                       Depth depth,
                       const ButterflyHistory* history,
                       const CounterMoveTable* counterMoves,
                       const KillerTable* killers,
                       int ply) noexcept
    : pos_(pos),
      history_(history),
      counterMoves_(counterMoves),
      killers_(killers),
      ttMove_(ttMove),
      depth_(depth),
      ply_(ply) {

    if (pos_.checkers()) {
        stage_ = STAGE_EVASION_TT;
    } else {
        stage_ = STAGE_MAIN_TT;
        if (killers_) {
            killer1_ = killers_->primary(ply_);
            killer2_ = killers_->secondary(ply_);
        }
        if (counterMoves_ && pos_.state()->previous) {
            const Move prevMove = pos_.state()->previous->currentMove;
            if (prevMove.is_ok()) {
                const Piece prevPc = pos_.piece_on(prevMove.to_sq());
                counterMove_ = counterMoves_->get(prevPc, prevMove.to_sq());
            }
        }
    }
}

MovePicker::MovePicker(const Position& pos,
                       Move ttMove,
                       const ButterflyHistory* history) noexcept
    : pos_(pos),
      history_(history),
      ttMove_(ttMove) {
    stage_ = STAGE_QSEARCH_TT;
}

void MovePicker::score_captures() noexcept {
    for (ExtMove* m = moves_; m < endMoves_; ++m) {
        const Piece victim = pos_.piece_on(m->to_sq());
        const PieceType attacker = Core::type_of(pos_.piece_on(m->from_sq()));

        if (m->type_of() == Core::EN_PASSANT) {
            m->value = 100 + 10000;
        } else if (victim != Core::NO_PIECE) {
            m->value = (VICTIM_SCORES[Core::type_of(victim)] * 10) - VICTIM_SCORES[attacker] + 10000;
        } else if (m->type_of() == Core::PROMOTION) {
            m->value = VICTIM_SCORES[m->promotion_type()] + 10000;
        } else {
            m->value = 0;
        }
    }
}

void MovePicker::score_quiets() noexcept {
    const Color us = pos_.side_to_move();
    for (ExtMove* m = moves_; m < endMoves_; ++m) {
        if (history_) {
            m->value = history_->get(us, m->from_sq(), m->to_sq());
        } else {
            m->value = 0;
        }
    }
}

void MovePicker::score_evasions() noexcept {
    const Color us = pos_.side_to_move();
    for (ExtMove* m = moves_; m < endMoves_; ++m) {
        const Piece victim = pos_.piece_on(m->to_sq());
        if (victim != Core::NO_PIECE) {
            const PieceType attacker = Core::type_of(pos_.piece_on(m->from_sq()));
            m->value = (VICTIM_SCORES[Core::type_of(victim)] * 10) - VICTIM_SCORES[attacker] + 10000;
        } else if (history_) {
            m->value = history_->get(us, m->from_sq(), m->to_sq());
        } else {
            m->value = 0;
        }
    }
}

Move MovePicker::select_best(ExtMove* begin, ExtMove* end) noexcept {
    ExtMove* best = begin;
    for (ExtMove* m = begin + 1; m < end; ++m) {
        if (m->value > best->value) {
            best = m;
        }
    }
    std::swap(*begin, *best);
    return *begin;
}

Move MovePicker::next_move() noexcept {
    while (stage_ != STAGE_DONE) {
        switch (stage_) {
            case STAGE_MAIN_TT:
            case STAGE_EVASION_TT:
            case STAGE_QSEARCH_TT: {
                stage_ = (stage_ == STAGE_MAIN_TT)    ? STAGE_CAPTURE_INIT :
                         (stage_ == STAGE_EVASION_TT) ? STAGE_EVASIONS_INIT :
                                                        STAGE_QCAPTURE_INIT;
                if (ttMove_.is_ok() && pos_.pseudo_legal(ttMove_)) {
                    return ttMove_;
                }
                break;
            }

            case STAGE_CAPTURE_INIT: {
                cur_ = moves_;
                endMoves_ = MoveGen::generate<CAPTURES>(pos_, moves_);
                score_captures();
                stage_ = STAGE_GOOD_CAPTURES;
                [[fallthrough]];
            }

            case STAGE_GOOD_CAPTURES: {
                while (cur_ < endMoves_) {
                    const Move m = select_best(cur_++, endMoves_);
                    if (m == ttMove_) continue;

                    if (pos_.see_ge(m, 0)) {
                        return m;
                    }
                    *badCaptures_-- = ExtMove(m, 0);
                }
                stage_ = STAGE_KILLER_1;
                break;
            }

            case STAGE_KILLER_1: {
                stage_ = STAGE_KILLER_2;
                if (killer1_.is_ok()
                    && killer1_ != ttMove_
                    && pos_.pseudo_legal(killer1_)
                    && !pos_.capture(killer1_)) {
                    return killer1_;
                }
                break;
            }

            case STAGE_KILLER_2: {
                stage_ = STAGE_COUNTERMOVE;
                if (killer2_.is_ok()
                    && killer2_ != ttMove_
                    && killer2_ != killer1_
                    && pos_.pseudo_legal(killer2_)
                    && !pos_.capture(killer2_)) {
                    return killer2_;
                }
                break;
            }

            case STAGE_COUNTERMOVE: {
                stage_ = STAGE_QUIET_INIT;
                if (counterMove_.is_ok()
                    && counterMove_ != ttMove_
                    && counterMove_ != killer1_
                    && counterMove_ != killer2_
                    && pos_.pseudo_legal(counterMove_)
                    && !pos_.capture(counterMove_)) {
                    return counterMove_;
                }
                break;
            }

            case STAGE_QUIET_INIT: {
                cur_ = moves_;
                endMoves_ = MoveGen::generate<QUIETS>(pos_, moves_);
                score_quiets();
                stage_ = STAGE_QUIET_MOVES;
                [[fallthrough]];
            }

            case STAGE_QUIET_MOVES: {
                while (cur_ < endMoves_) {
                    const Move m = select_best(cur_++, endMoves_);
                    if (m == ttMove_ || m == killer1_ || m == killer2_ || m == counterMove_) {
                        continue;
                    }
                    return m;
                }
                stage_ = STAGE_BAD_CAPTURES;
                cur_ = badCaptures_ + 1;
                endMoves_ = moves_ + Core::MAX_MOVES;
                break;
            }

            case STAGE_BAD_CAPTURES: {
                if (cur_ < endMoves_) {
                    return *(cur_++);
                }
                stage_ = STAGE_DONE;
                break;
            }

            case STAGE_EVASIONS_INIT: {
                cur_ = moves_;
                endMoves_ = MoveGen::generate<EVASIONS>(pos_, moves_);
                score_evasions();
                stage_ = STAGE_EVASIONS;
                [[fallthrough]];
            }

            case STAGE_EVASIONS: {
                while (cur_ < endMoves_) {
                    const Move m = select_best(cur_++, endMoves_);
                    if (m != ttMove_) {
                        return m;
                    }
                }
                stage_ = STAGE_DONE;
                break;
            }

            case STAGE_QCAPTURE_INIT: {
                cur_ = moves_;
                endMoves_ = MoveGen::generate<CAPTURES>(pos_, moves_);
                score_captures();
                stage_ = STAGE_QCAPTURES;
                [[fallthrough]];
            }

            case STAGE_QCAPTURES: {
                while (cur_ < endMoves_) {
                    const Move m = select_best(cur_++, endMoves_);
                    if (m != ttMove_ && pos_.see_ge(m, 0)) {
                        return m;
                    }
                }
                stage_ = STAGE_DONE;
                break;
            }

            case STAGE_DONE:
                return Move::none();
        }
    }

    return Move::none();
}

} // namespace Engine::MoveGen