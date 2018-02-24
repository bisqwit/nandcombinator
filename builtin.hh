#include <type_traits>
#include <cstdint>

#ifdef __GNUC__
# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)
# define assume_aligned(ptr, n) ((decltype(ptr))  __builtin_assume_aligned(ptr, n))
#else
# define likely(x)   (x)
# define unlikely(x) (x)
# define assume_aligned(ptr, n) ptr
#endif

#if __cplusplus >= 201402L && __GNUC__ >= 7
 #define if_constexpr(x) if constexpr(x)
#else
 #define if_constexpr(x) if(x)
#endif

#pragma GCC push_options
#pragma GCC optimize ("Ofast")

/* Find the index of the lowest-order one-bit in the value. */
template<typename T>
unsigned FindFirstBit(T value)
{
    static_assert(std::is_unsigned<T>::value, "FindFirstBit only works with unsigned values");
    if(!value) return sizeof(T)*8;
#ifdef __GNUC__
    if_constexpr(sizeof(value) > sizeof(long) && sizeof(value) <= sizeof(long long))
    {
        unsigned long long v = value;
        return __builtin_ctzll(v);
    }
    if_constexpr(sizeof(value) > sizeof(int) && sizeof(value) <= sizeof(long))
    {
        unsigned long v = value;
        return __builtin_ctzl(v);
    }
    if_constexpr(sizeof(value) <= sizeof(int))
    {
        unsigned int v = value;
        return __builtin_ctz(v);
    }
#endif
    unsigned result = 0;
    if_constexpr(sizeof(value) > 4) { if(!(value & 0xFFFFFFFFul)) { result += 32; { value >>= 16; value >>= 16; } } }
    if_constexpr(sizeof(value) > 2) { if(!(value &     0xFFFFul)) { result += 16; value >>= 16; } }
    if_constexpr(sizeof(value) > 1) { if(!(value &       0xFFul)) { result +=  8; value >>=  8; } }
    /*******************************/ if(!(value &        0xFul)) { result +=  4; value >>=  4; }
    /*******************************/ if(!(value &        0x3ul)) { result +=  2; value >>=  2; }
    /*******************************/ if(!(value &        0x1ul)) { result +=  1; value >>=  1; }
    return result;
}

/* Find the index of the highest-order one-bit in the value. */
template<typename T>
unsigned FindLastBit(T value)
{
    static_assert(std::is_unsigned<T>::value, "FindLastit only works with unsigned values");
    if(!value) return sizeof(T)*8;
#ifdef __GNUC__
    if_constexpr(sizeof(value) > sizeof(long) && sizeof(value) <= sizeof(long long))
    {
        unsigned long long v = value;
        return 8*sizeof(v)-1 - __builtin_clzll(v);
    }
    if_constexpr(sizeof(value) > sizeof(int) && sizeof(value) <= sizeof(long))
    {
        unsigned long v = value;
        return 8*sizeof(v)-1 - __builtin_clzl(v);
    }
    if_constexpr(sizeof(value) <= sizeof(int))
    {
        unsigned int v = value;
        return 8*sizeof(v)-1 - __builtin_clz(v);
    }
#endif
    unsigned leading_zeros = 0;
    if_constexpr(sizeof(value) > 4) { if(!(value & 0xFFFFFFFF00000000ull)) leading_zeros += 32; else { value >>= 16; value >>= 16; } }
    if_constexpr(sizeof(value) > 2) { if(!(value >> 16)) leading_zeros += 16; else value >>= 16; }
    if_constexpr(sizeof(value) > 1) { if(!(value >> 8))  leading_zeros += 8;  else value >>= 8; }
    /*******************************/ if(!(value >> 4))  leading_zeros += 4;  else value >>= 4;
    /*******************************/ if(!(value >> 2))  leading_zeros += 2;  else value >>= 2;
    /*******************************/ if(!(value >> 1))  leading_zeros += 1; /*else value >>= 1;*/
    return sizeof(T)*8-1 - leading_zeros;
}

/* Find the index of the lowest-order zero-bit in the value. */
template<typename T>
unsigned FindFirstZeroBit(T value)
{
    return FindFirstBit(T(~value));
}

template<typename T>
unsigned FindLastZeroBit(T value)
{
    return FindLastBit(T(~value));
}

/* From https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel */
inline std::uint_fast8_t PopulationCount(std::uint_least32_t v)
{
#ifdef __GNUC__
    return __builtin_popcount(v);
#else
    v = v - ((v >> 1) & 0x55555555u);                          // reuse input as temporary
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);          // temp
    return (((v + (v >> 4)) & 0xF0F0F0Fu) * 0x1010101u) >> 24; // count
#endif
}

inline std::uint_fast8_t PopulationCount(std::uint_fast64_t v)
{
#ifdef __GNUC__
    return __builtin_popcountll(v);
#else
    std::uint_least32_t low = v & 0xFFFFFFFFul, high = v >> 32;
    return PopulationCount(low) + PopulationCount(high);
#endif
}

#undef if_constexpr

#pragma GCC pop_options
