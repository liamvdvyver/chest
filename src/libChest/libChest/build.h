//============================================================================//
// Build information
//============================================================================//

#ifdef NDEBUG
#define DEBUG() false
#else
#define DEBUG() true
#endif

#if __has_builtin(__builtin_ctzll)
#define BITSCAN() true
#else
#define BITSCAN() false
#endif

#if __has_builtin(__builtin_popcountll)
#define POPCOUNT() true
#else
#define POPCOUNT() false
#endif

// If x86 pext instruction is present,
// it is used for generating precomputed attack keys.
#if defined(__BMI2__)
#define PEXT() true
#else
#define PEXT() false
#endif
