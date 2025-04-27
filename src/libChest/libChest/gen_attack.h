

// For the sliding pieces we have a few approches to storing precomputed moves,
// along two directions:
//
// 1. Key calculation:
// * Magic bitboard multiplication
// * Pext (x86)
//
// 2. Move storage:
// * Constant shift: 2D stack array
// * Variable shift: 2D heap array + lookup for key width
// * Variable shift: stack array + lookup for starting offset/key width
//
// So, we have two templates + a concept for the key calculation,
// and three templates for move storage, which satisfy attacker.
//
// This is for easy testing, the fastest combination per-platform will be
// typedef'd with directives.

template <typename T>
concept PrecomputedStorage =
    requires(T t, board::Square sq, board::Bitboard occ, size_t key) {
        { t.at(sq, key) } -> std::same_as<board::Bitboard &>;
        { t.get_mask(sq) } -> std::same_as<board::Bitboard>;

        // Not precomputed: used for initialisation only
        { t.get_attack_live(sq, occ) } -> std::same_as<board::Bitboard>;
        { t.get_key_width(sq) } -> std::same_as<size_t>;
    };

template <typename T>
concept KeyGen = requires(T t, board::Square sq, board::Bitboard blk,
                          board::Bitboard occ, size_t key_width) {
    { t.key(sq, blk, occ, key_width) } -> std::same_as<size_t>;
    { t.init() } -> std::same_as<void>;
};

// template <typename T>
// concept PrecomputedSlidingAttacker = requires(T t) {
//
// }

// Magic numbers
// typedef board::Bitboard magic_t;
//
// template <PrecomputedStorage TPrecomputed, SlidingAttacker TAttacker,
//           Attacker TMasker>
// class MagicKeyGen {
//   public:
//     constexpr MagicKeyGen(const TAttacker &attacker, const TMasker &masker,
//                           TPrecomputed precomp)
//         : m_precomp(precomp), m_attacker(attacker), m_masker(masker) {};
//
//     constexpr size_t key(board::Square sq, board::Bitboard blk,
//                          board::Bitboard occ, size_t key_width) const {
//         return key(blk, occ, m_magic[sq], key_width);
//     }
//
//     constexpr size_t key(board::Bitboard blk, board::Bitboard occ,
//                          magic_t magic, size_t key_width) const {
//         return ((board::bitboard_t)((occ & blk) * magic) >>
//                 (board::n_squares - key_width));
//     }
//
//     // Assume m_precomp is already initalised
//     void init() {
//         init_magics();
//     }
//
//   private:
//     // Hold reference to precomputed generator to populate attack map
//     TPrecomputed &m_precomp;
//
//     // Hold references to attack/mask generators
//     const TAttacker &m_attacker;
//     const TMasker &m_masker;
//
//     // Generated magics
//     magic_t m_magic[board::n_squares];
//
//     // Populate all magic numbers
//     constexpr void init_magics() {
//
//         magic_t magic_num;
//         bool done;
//
//         std::random_device rd;
//         std::mt19937_64 eng(rd());
//
//         std::uniform_int_distribution<board::bitboard_t> rand;
//
//         std::vector<size_t> visited;
//
//         for (board::Square sq : board::Square::AllSquareIterator()) {
//             done = false;
//             do {
//                 magic_num = rand(eng) & rand(eng) & rand(eng);
//                 done = init_attacks(magic_num, sq, visited);
//             } while (!done);
//             m_magic[sq] = magic_num;
//         }
//     };
//
//     // Populate attack maps for a position, given a candidate magic number.
//     // Returns whether successful.
//     constexpr bool init_attacks(magic_t magic, board::Square sq,
//                                 board::Bitboard blk,
//                                 std::vector<size_t> &visited) {
//         visited.clear();
//
//         size_t key = 0;
//
//         for (board::Bitboard blocker_subset : blk.subsets()) {
//
//             board::Bitboard attacked =
//                 m_precomp.get_attack_live(sq, blocker_subset);
//             key = key(blk, blocker_subset, magic, m_precomp.get_key_width(sq));
//             board::Bitboard &cur_elm = m_precomp.at(sq, key);
//
//             // Collision with different val
//             if (!cur_elm.empty() && cur_elm != attacked) {
//
//                 for (const size_t visited_key : visited) {
//                     m_precomp.at(sq, visited_key) = 0;
//                 }
//
//                 return false;
//             }
//
//             visited.push_back(key);
//             cur_elm = attacked;
//         }
//
//         return true;
//     };
// };

// template <size_t attack_sz, SlidingAttacker TAttacker, Attacker TMasker>
// class FixedShiftSlider<> {
//
//   public:
//     constexpr FixedShiftSlider() : m_info() { init(); };
//
//     constexpr void init() {
//         init_masks();
//         init_shifts();
//         init_magics();
//     }
//
//     // Get all squares attacked by a piece.
//     // May include own pieces.
//     constexpr board::Bitboard operator()(board::Square sq,
//                                          board::Bitboard occ) const {
//         return m_attacks.at(get_magic_key(sq, occ));
//     };
//
//   private:
//     //
//     // Piece-specific.
//     //
//
//     struct MagicInfo {
//         magic_t magic;
//         uint8_t shift_width;
//         board::Bitboard blocker_mask;
//
//         // Offset into attack map
//         size_t base_offset;
//     };
//
//     // Number of possible attack keys
//     // static const int n_attack_keys = 1 << (max_shift);
//
//     // Generate blocker mask
//     TMasker m_masker;
//     constexpr board::Bitboard const gen_mask(board::Square sq) {
//         return m_masker(sq);
//     };
//
//     // Generate attacks given attacker square and (relevant or total) blocker
//     // occupancy
//     TAttacker m_attacker;
//     constexpr board::Bitboard const gen_attacks(board::Square sq,
//                                                 board::Bitboard blk) {
//         return m_attacker(sq, blk);
//     };
//
//     //
//     // Precomputed-data
//     //
//
//     std::array<MagicInfo, board::n_squares> m_info;
//     svec<board::Bitboard, attack_sz> m_attacks;
//
//     //
//     // Initialisation
//     //
//
//
//     // Populate all blocker masks
//     constexpr void init_masks() {
//         for (board::Square sq : board::Square::AllSquareIterator()) {
//             board::Bitboard mask = gen_mask(sq);
//             m_info[sq].blocker_mask = mask;
//         }
//     };
//
//
//     // Populate all attack map key sizes
//     constexpr void init_shifts() {
//         for (board::Square sq : board::Square::AllSquareIterator()) {
//             size_t cur_sz = m_info[sq].blocker_mask.size();
//             m_info[sq].shift_width = cur_sz;
//             m_info[sq].base_offset = m_attacks.size();
//             m_attacks.resize(m_attacks.size() + (1 << cur_sz));
//         }
//         assert(m_info[board::n_squares - 1].base_offset +
//                    (1 << m_info[board::n_squares - 1].shift_width) ==
//                attack_sz);
//     };
//
//     //
//     // Lookup
//     //
//
//     // Get the attack map key, given position, blockers (relevant or total)
//     constexpr size_t get_magic_key(board::Square sq,
//                                    board::Bitboard occ) const {
//         return get_magic_key(sq, occ, m_info[sq].magic);
//     };
//
//     // Helper
//     // Plain magic bitboard -> constant shift
//     constexpr int shift_width(board::Square sq) const {
//         // (void)sq;
//         // return board::n_squares - max_shift;
//         // For fancy bitboards:
//         return board::n_squares - m_info[sq].shift_width;
//     }
//
// };


// Given attack and mask generators, implement plain magic bitboards.
// Parameterise key (bit) size for attacks to make array size concrete.
// TODO: test out fancy (variable shift) bitboards.
// template <size_t attack_sz, SlidingAttacker TAttacker, Attacker TMasker>
// class MagicAttacker {
//
//   public:
//     constexpr MagicAttacker() : m_info() { init(); };
//
//     constexpr void init() {
//         init_masks();
//         init_shifts();
//         init_magics();
//     }
//
//     // Get all squares attacked by a piece.
//     // May include own pieces.
//     constexpr board::Bitboard operator()(board::Square sq,
//                                          board::Bitboard occ) const {
//         return m_attacks.at(get_magic_key(sq, occ));
//     };
//
//   private:
//     //
//     // Piece-specific.
//     //
//
//     struct MagicInfo {
//         magic_t magic;
//         uint8_t shift_width;
//         board::Bitboard blocker_mask;
//
//         // Offset into attack map
//         size_t base_offset;
//     };
//
//     // Number of possible attack keys
//     // static const int n_attack_keys = 1 << (max_shift);
//
//     // Generate blocker mask
//     TMasker m_masker;
//     constexpr board::Bitboard const gen_mask(board::Square sq) {
//         return m_masker(sq);
//     };
//
//     // Generate attacks given attacker square and (relevant or total) blocker
//     // occupancy
//     TAttacker m_attacker;
//     constexpr board::Bitboard const gen_attacks(board::Square sq,
//                                                 board::Bitboard blk) {
//         return m_attacker(sq, blk);
//     };
//
//     //
//     // Precomputed-data
//     //
//
//     std::array<MagicInfo, board::n_squares> m_info;
//     svec<board::Bitboard, attack_sz> m_attacks;
//
//     //
//     // Initialisation
//     //
//
//
//     // Populate all blocker masks
//     constexpr void init_masks() {
//         for (board::Square sq : board::Square::AllSquareIterator()) {
//             board::Bitboard mask = gen_mask(sq);
//             m_info[sq].blocker_mask = mask;
//         }
//     };
//
//
//     // Populate all attack map key sizes
//     constexpr void init_shifts() {
//         for (board::Square sq : board::Square::AllSquareIterator()) {
//             size_t cur_sz = m_info[sq].blocker_mask.size();
//             m_info[sq].shift_width = cur_sz;
//             m_info[sq].base_offset = m_attacks.size();
//             m_attacks.resize(m_attacks.size() + (1 << cur_sz));
//         }
//         assert(m_info[board::n_squares - 1].base_offset +
//                    (1 << m_info[board::n_squares - 1].shift_width) ==
//                attack_sz);
//     };
//
//     //
//     // Lookup
//     //
//
//     // Get the attack map key, given position, blockers (relevant or total)
//     constexpr size_t get_magic_key(board::Square sq,
//                                    board::Bitboard occ) const {
//         return get_magic_key(sq, occ, m_info[sq].magic);
//     };
//
//     // Helper
//     // Plain magic bitboard -> constant shift
//     constexpr int shift_width(board::Square sq) const {
//         // (void)sq;
//         // return board::n_squares - max_shift;
//         // For fancy bitboards:
//         return board::n_squares - m_info[sq].shift_width;
//     }
//
// };
//
template <size_t attack_sz, SlidingAttacker TAttacker, Attacker TMasker>
class PextAttacker {

  public:
    constexpr PextAttacker() : m_info() { init(); };

    constexpr void init() {
        init_masks();
        init_shifts();
        init_attacks();
    }

    // Get all squares attacked by a piece.
    // May include own pieces.
    constexpr board::Bitboard operator()(board::Square sq,
                                         board::Bitboard occ) const {
        return m_attacks.at(get_attack_key(sq, occ));
    };

  private:
    int max_shift;
    struct PextInfo {
        uint8_t shift_width;
        board::Bitboard blocker_mask;
        size_t base_offset;
    };

    // Number of possible attack keys
    // static const int n_attack_keys = 1 << (max_shift);

    // Generate blocker mask
    TMasker m_masker;
    constexpr board::Bitboard const gen_mask(board::Square sq) {
        return m_masker(sq);
    };

    // Generate attacks given attacker square and (relevant or total) blocker
    // occupancy
    TAttacker m_attacker;
    constexpr board::Bitboard const gen_attacks(board::Square sq,
                                                board::Bitboard blk) {
        return m_attacker(sq, blk);
    };

    //
    // Precomputed-data
    //

    std::array<PextInfo, board::n_squares> m_info;
    svec<board::Bitboard, attack_sz> m_attacks;

    //
    // Initialisation
    //

    constexpr void init_attacks() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            init_attacks(sq);
        }
    }

    // Populate attack maps for a position, given a candidate magic number.
    // Returns whether successful.
    constexpr bool init_attacks(board::Square sq) {
        board::Bitboard all_blockers = m_info[sq].blocker_mask;

        size_t key = 0;

        for (board::Bitboard blocker_subset : all_blockers.subsets()) {

            board::Bitboard attacked = gen_attacks(sq, blocker_subset);
            key = get_attack_key(sq, blocker_subset);
            board::Bitboard &cur_elm = m_attacks.at(key);
            cur_elm = attacked;
        }

        return true;
    };

    // Populate all blocker masks
    constexpr void init_masks() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            board::Bitboard mask = gen_mask(sq);
            m_info[sq].blocker_mask = mask;
        }
    };

    // Populate all attack map key sizes
    constexpr void init_shifts() {
        for (board::Square sq : board::Square::AllSquareIterator()) {
            size_t cur_sz = m_info[sq].blocker_mask.size();
            if cur_sz >=
            m_info[sq].shift_width = cur_sz;
            m_info[sq].base_offset = m_attacks.size();
            m_attacks.resize(m_attacks.size() + (1 << cur_sz));
        }
        assert(m_info[board::n_squares - 1].base_offset +
                   (1 << m_info[board::n_squares - 1].shift_width) ==
               attack_sz);
    };

    //
    // Lookup
    //

    // Get the attack map key, given position, blockers (relevant or total)
    constexpr size_t get_attack_key(board::Square sq,
                                    board::Bitboard occ) const {
        return m_info[sq].base_offset +
               _pext_u64((board::bitboard_t)occ,
                         (board::bitboard_t)m_info[sq].blocker_mask);
    };

    // Helper
    constexpr int shift_width(board::Square sq) const {
        // return board::n_squares - m_info[sq].shift_width;
        return max_shift;
    }
};
