.
├── CMakeLists.txt
├── README.md
├── configs/                          # Configurable search & tuning presets
│   ├── default.json
│   └── spsa_params.json
├── weights/                          # Embedded / external NNUE network files
│   └── default.nnue
├── scripts/                          # Benchmarking, perft suites, SPSA tuning scripts
│   ├── download_nets.sh
│   └── spsa_tune.py
├── tests/                            # Unit tests, Perft suites, Benchmarks
│   ├── CMakeLists.txt
│   ├── test_bitboards.cpp
│   ├── test_movegen_perft.cpp
│   ├── test_search.cpp
│   └── test_nnue_simd.cpp
└── src/
    ├── CMakeLists.txt
    ├── main.cpp                      # Minimal entrypoint
    │
    ├── core/                         # Fundamental primitives & types (zero runtime dependencies)
    │   ├── types.h                   # Square, Piece, Color, Move, Value, CastlingRights
    │   ├── bitboard.h/.cpp           # Bitboard bitwise ops, popcount, ctz, magic/PEXT tables
    │   ├── zobrist.h/.cpp            # Zobrist hash keys & initialization
    │   └── intrinsics.h              # Compiler/CPU intrinsics (AVX2, AVX-512, BMI2, NEON wrappers)
    │
    ├── position/                     # Board state & mutation
    │   ├── position.h/.cpp           # Board representation, state stack, make/unmake
    │   ├── attacks.h/.cpp            # Fast lookup for pawn/knight/king & slider ray attacks
    │   └── fen.h/.cpp                # FEN parsing, serialization, and validation
    │
    ├── movegen/                      # Move generation & staging
    │   ├── movegen.h/.cpp            # Pseudo-legal move generator (captures, quiets, checks)
    │   ├── movepicker.h/.cpp         # Move ordering state machine for alpha-beta
    │   └── perft.h/.cpp              # Node counting / perft debugging utility
    │
    ├── search/                       # Minimax, Alpha-Beta, Pruning & History
    │   ├── search.h/.cpp             # Iterative deepening, PVS, root search
    │   ├── tt.h/.cpp                 # Lockless Transposition Table & Cluster layout
    │   ├── history.h                 # Butterfly history, Continuation history, Killer moves
    │   ├── reductions.h              # LMR, NMP, Reverse Futility, ProbCut formulas
    │   └── timeman.h/.cpp            # Time management allocation formulas
    │
    ├── eval/                         # Position evaluation
    │   ├── evaluator.h               # Abstract / unified evaluation interface
    │   ├── classical/                # Baseline Hand-Crafted Evaluation (HCE) for fallback & tests
    │   │   ├── material.h/.cpp
    │   │   └── psqt.h/.cpp           # Piece-Square Tables & mobility
    │   └── nnue/                     # Efficiently Updatable Neural Network
    │       ├── accumulator.h/.cpp    # Incremental update accumulator on state stack
    │       ├── architecture.h        # Network layer definitions (e.g. HalfKA, Dual-accumulators)
    │       ├── layers/               # Affine transform, ClippedReLU, SIMD kernels
    │       │   ├── affine.h
    │       │   ├── activations.h
    │       │   └── simd_vector.h     # Clean vectorization wrappers (AVX2/AVX512/NEON)
    │       └── loader.h/.cpp         # Reading binary weights / incbin embedding
    │
    ├── parallel/                     # Concurrency & threading
    │   ├── thread_pool.h/.cpp        # Worker thread lifecycle management
    │   ├── lazy_smp.h/.cpp           # Lazy SMP search coordination
    │   └── numa.h/.cpp               # NUMA awareness / memory binding (optional for high-core count)
    │
    ├── endgame/                      # Tablebase probing
    │   └── syzygy.h/.cpp             # Syzygy WDL & DTZ tablebase probe wrapper
    │
    ├── uci/                          # Protocol & engine interface
    │   ├── uci.h/.cpp                # UCI protocol parser & dispatch loop
    │   ├── engine.h/.cpp             # Main engine coordinator exposing high-level API
    │   └── options.h/.cpp            # Dynamic UCI options (Hash, Threads, MultiPV, NNUE file)
    │
    └── utils/                        # System utilities & tuning hooks
        ├── tuning.h/.cpp             # Compile-time / runtime SPSA tuning parameter registry
        ├── benchmark.h/.cpp          # Standardized bench command execution
        └── memory.h/.cpp             # Large page / aligned allocator helpers