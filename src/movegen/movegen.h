#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>

#include "core/types.h"
#include "position/position.h"
#include "utils/stack_vector.h"

namespace Engine::MoveGen {

using Core::ExtMove;
using Core::Move;
using Core::Square;
using Core::Piece;
using Core::Color;
using Core::MAX_MOVES;
using Position = Engine::Position::Position;

enum GenType {
    CAPTURES,
    QUIETS,
    EVASIONS,
    NON_EVASIONS,
    LEGAL
};

template<GenType Type>
Move* generate(const Position& pos, Move* moveList) noexcept;

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) noexcept;

// Stack-allocated MoveList container with STL iterator support
template<GenType Type>
class MoveList {
public:
    explicit MoveList(const Position& pos) noexcept
        : last_(generate<Type>(pos, moveList_.data())) {}

    [[nodiscard]] const Move* begin() const noexcept { return moveList_.data(); }
    [[nodiscard]] const Move* end() const noexcept { return last_; }
    [[nodiscard]] Move* begin() noexcept { return moveList_.data(); }
    [[nodiscard]] Move* end() noexcept { return last_; }

    [[nodiscard]] std::size_t size() const noexcept {
        return static_cast<std::size_t>(last_ - moveList_.data());
    }
    [[nodiscard]] bool empty() const noexcept { return last_ == moveList_.data(); }

    [[nodiscard]] bool contains(Move move) const noexcept {
        return std::find(begin(), end(), move) != end();
    }

    [[nodiscard]] Move operator[](std::size_t index) const noexcept {
        assert(index < size());
        return moveList_[index];
    }

    [[nodiscard]] Move& operator[](std::size_t index) noexcept {
        assert(index < size());
        return moveList_[index];
    }

private:
    std::array<Move, MAX_MOVES> moveList_{};
    Move* last_{nullptr};
};

} // namespace Engine::MoveGen