#include <iostream>
#include "core/bitboard.h"
#include "position/position.h"
#include "movegen/perft.h"

int main() {
    Engine::Core::Bitboards::init();
    Engine::Position::Position::init();

    return Engine::MoveGen::run_full_perft_suite() ? 0 : 1;
}