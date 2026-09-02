#include "core/bitboard.h"

#include <algorithm>
#include <string>

namespace Engine::Core {

uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard RayPassBB[SQUARE_NB][SQUARE_NB];

alignas(64) Magic Magics[SQUARE_NB][2];

namespace {

Bitboard RookTable[0x19000];   // Stores rook attack lookups (~102k entries)
Bitboard BishopTable[0x1480];  // Stores bishop attack lookups (~5.2k entries)

// Fast 64-bit SplitMix PRNG to discover valid magics without external dependencies
class SimplePRNG {
public:
    constexpr explicit SimplePRNG(uint64_t seed) noexcept : state(seed) {}

    constexpr uint64_t rand64() noexcept {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    constexpr uint64_t sparse_rand64() noexcept {
        return rand64() & rand64() & rand64();
    }

private:
    uint64_t state;
};

void init_magics(PieceType pt, Bitboard table[], Magic magics[][2]) noexcept {
    Bitboard reference[4096];
#if !defined(USE_PEXT)
    Bitboard occupancy[4096];
    int epoch[4096] = {};
    int cnt = 0;
#endif
    int size = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        const Bitboard edges = ((Rank1BB | Rank8BB) & ~rank_bb(s)) 
                             | ((FileABB | FileHBB) & ~file_bb(s));

        Magic& m = magics[s][pt - BISHOP];
        m.mask   = Bitboards::sliding_attack(pt, s, 0) & ~edges;
#if !defined(USE_PEXT)
        m.shift  = 64 - popcount(m.mask);
#endif
        m.attacks = (s == SQ_A1) ? table : magics[s - 1][pt - BISHOP].attacks + size;
        size      = 0;

        // Enumerate subsets of mask using the Carry-Rippler algorithm
        Bitboard b = 0;
        do {
#if !defined(USE_PEXT)
            occupancy[size] = b;
#endif
            reference[size] = Bitboards::sliding_attack(pt, s, b);

            if constexpr (HasPext) {
                m.attacks[pext64(b, m.mask)] = reference[size];
            }

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

#if !defined(USE_PEXT)
        SimplePRNG rng(20260401ULL + static_cast<uint64_t>(s) * 31ULL);

        for (int i = 0; i < size;) {
            for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
                m.magic = rng.sparse_rand64();

            for (++cnt, i = 0; i < size; ++i) {
                const unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt) {
                    epoch[idx]     = cnt;
                    m.attacks[idx] = reference[i];
                } else if (m.attacks[idx] != reference[i]) {
                    break;
                }
            }
        }
#endif
    }
}

} // namespace

void Bitboards::init() noexcept {
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2) {
            SquareDistance[s1][s2] = static_cast<uint8_t>(
                std::max(distance<File>(s1, s2), distance<Rank>(s1, s2))
            );
        }
    }

    init_magics(ROOK, RookTable, Magics);
    init_magics(BISHOP, BishopTable, Magics);

    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1) {
        for (const PieceType pt : {BISHOP, ROOK}) {
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2) {
                if (PseudoAttacks[pt][s1] & s2) {
                    LineBB[s1][s2] = (attacks_bb(pt, s1, 0) & attacks_bb(pt, s2, 0)) | s1 | s2;
                    BetweenBB[s1][s2] = (attacks_bb(pt, s1, square_bb(s2)) & attacks_bb(pt, s2, square_bb(s1)));
                    RayPassBB[s1][s2] = attacks_bb(pt, s1, 0) & (attacks_bb(pt, s2, square_bb(s1)) | s2);
                }
                BetweenBB[s1][s2] |= s2;
            }
        }
    }
}

std::string Bitboards::pretty(Bitboard b) {
    std::string s = "+---+---+---+---+---+---+---+---+\n";

    for (int r = static_cast<int>(RANK_8); r >= static_cast<int>(RANK_1); --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
            const Square sq = make_square(f, static_cast<Rank>(r));
            s += (b & sq) ? "| X " : "|   ";
        }
        s += "| " + std::to_string(r + 1) + "\n+---+---+---+---+---+---+---+---+\n";
    }
    s += "  a   b   c   d   e   f   g   h\n";
    return s;
}

} // namespace Engine::Core