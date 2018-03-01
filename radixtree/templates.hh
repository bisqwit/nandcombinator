#ifndef bqtPbTemplatesHH
#define bqtPbTemplatesHH

#include <vector> // size_t
#include <stdint.h>

#ifndef __x86_64
# define REGPARM __attribute__((regparm(3)))
#else
# define REGPARM
#endif

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

using size_t = std::size_t;

template <int T>
struct BinaryComponentSize
{
    enum { _Divisible = !(T&1),
           Result = _Divisible ? BinaryComponentSize<T/2>::Result+1 : 0 };
};
template <> struct BinaryComponentSize<0> { enum { Result=0 }; };
template <> struct BinaryComponentSize<1> { enum { Result=0 }; };

template<size_t p,size_t v> struct IntExp
    { static const size_t Result = v * IntExp<p-1,v>::Result; };
template<size_t v> struct IntExp<1,v> { static const size_t Result = v; };
template<size_t v> struct IntExp<0,v> { static const size_t Result = 1; };

template<size_t T> struct Log2Floor { enum { Bits = Log2Floor<T/2>::Bits + 1 }; };
template<> struct Log2Floor<1> { enum { Bits = 0 }; };
template<> struct Log2Floor<0> { enum { Bits = -1 }; };
template<size_t T> struct Log2Ceil
{
    enum { PowerOf2 = !(T&(T-1)), Bits = Log2Floor<T/2>::Bits + 2 - PowerOf2 };
};

template <size_t T, size_t Exp>
struct Powvalue
{
    enum { _Binary = BinaryComponentSize<T>::Result,
           Shift = _Binary*Exp };
    static const size_t Mul = IntExp<Exp, (T >> _Binary)>::Result;
};

template<int T> struct OptimalTypePerRemainder { typedef unsigned char Type; };

/*template<> struct OptimalTypePerRemainder<sizeof(long long)%sizeof(size_t)>
    { typedef unsigned long long Type; };*/
template<> struct OptimalTypePerRemainder<sizeof(long)%sizeof(size_t)>
    { typedef unsigned long Type; };
template<> struct OptimalTypePerRemainder<sizeof(short)%sizeof(size_t)>
    { typedef unsigned short Type; };

template<int Nbytes> struct OptimalStorageType
    { typedef typename OptimalTypePerRemainder<Nbytes%sizeof(size_t)>::Type Type; };

template<int T> struct CoordTraits { };
template<> struct CoordTraits<1> { typedef signed char  Type; };
template<> struct CoordTraits<2> { typedef signed short Type; };
template<> struct CoordTraits<3> { typedef int_fast32_t Type; };
template<> struct CoordTraits<4> { typedef int_fast32_t Type; };
template<> struct CoordTraits<5> { typedef int_fast64_t Type; };
template<> struct CoordTraits<6> { typedef int_fast64_t Type; };
template<> struct CoordTraits<7> { typedef int_fast64_t Type; };
template<> struct CoordTraits<8> { typedef int_fast64_t Type; };

template<int LargestValueByteSize> struct MinimumSizeType { typedef unsigned long Type; };
template<> struct MinimumSizeType<4> { typedef unsigned Type; };
template<> struct MinimumSizeType<3> { typedef unsigned Type; };
template<> struct MinimumSizeType<2> { typedef unsigned short Type; };
template<> struct MinimumSizeType<1> { typedef unsigned char  Type; };

template<size_t T>
struct Log256Ceil
{
    enum { Result = (Log2Ceil<T>::Bits + 7) / 8 };
};

template<size_t p> struct Max2Exp
{
    enum { Result = (sizeof(size_t)*8-1) / Log2Ceil<p>::Bits };
};
template<size_t p> struct Max256Exp
{
    enum { Result = (sizeof(size_t)*8-1) / Log256Ceil<p>::Bits };
};

template<size_t OuterLimit, size_t MaxCount, size_t BitUnit>
struct UnitsUsedBy
{
    enum { MaxBits = 
      (MaxCount * (Log2Ceil<
                    IntExp<Max2Exp<OuterLimit>::Result, OuterLimit>::Result
                        >::Bits) / Max2Exp<OuterLimit>::Result) };
    enum { MaxBytes = (MaxBits + BitUnit - 1) / BitUnit };
};

template<size_t OuterLimit, size_t MaxCount>
struct BytesUsedBy: public UnitsUsedBy<OuterLimit, MaxCount, 8> { };


template<size_t NBitsTotal, size_t NewRadix>
struct ChangeRadix
{
    // Calculate NBitsTotal * log(2) / log(NewRadix)
    //         = NBitsTotal * (log_NewRadix(2))
    //         = NBitsTotal / log2(NewRadix)
    //         = NBitsTotal * N / log2(NewRadix^N)
    //enum { maxexp = Max2Exp<NewRadix>::Result }; // Largest value NewRadix can be exponentiated to safely
    //enum { log = Log2Floor<IntExp<maxexp, NewRadix>::Result>::Bits };
    //enum { Result = (NBitsTotal * maxexp + log-1) / log };
    enum { Result = (NBitsTotal + NewRadix-1) / NewRadix };
    // Purpose: 2^NBitsTotal is the maximum value of the input integer.
    // That is, NBitsTotal tells the number of bits required to store the value.
    // Output is the number of NewRadix-bit elements needed to store the value.
};


static inline unsigned CalcBytesNeededByInt(size_t Value)
{
#if defined(__x86_64)
    if(Value >> 32)
    {
        if(Value >> 48)
        {
            return (Value >> 56) ? 8 : 7;
        }
        return (Value >> 40) ? 6 : 5;
    }
#endif
    if(         Value & (0xFFFF0000UL))
    {
        return (Value & (0xFF000000UL)) ? 4 : 3;
    }
    return (Value & 0xFF00U) ? 2 : 1;
}

#endif
