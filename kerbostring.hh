#include <cstdint>
#include <array>
#include <stdexcept>
#include <sys/mman.h>
#include <cassert>
#include <cstring>

/* KerboString is a version of std::string that has a small memory footprint.
 * It also uses pool allocation.
 */


/*
    Max length on 16 inputs, 16 outputs: 1048576 bits -> 131072 bytes (17 bits)
    20 bits for length
    44 bits remain for offset (=167 TB)
    divide into pools of 4 GB (=32 bits)
    12 bits remain for pool index = 4096 pools
*/
template<std::size_t MaxLength>
class KerboPool
{
    char*       data = nullptr;
    std::size_t cap  = 0;
public:
    KerboPool()
    {
    }
    ~KerboPool()
    {
        if(data)
            munmap(data, MaxLength);
    }
    const char* GetReadOffset(std::size_t offset) const
    {
        return data + offset;
    }
    char* GetWriteOffset(std::size_t offset)
    {
        if(!data)
        {
            data = (char*)mmap64(nullptr, MaxLength, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        }
        return data + offset;
    }
    std::size_t Allocate(std::size_t initial_size)
    {
        std::size_t result = cap;
        cap += initial_size;
        return result;
    }
    void Free(std::size_t offset, std::size_t length)
    {
        if(offset+length == cap) cap = offset;
    }
    std::size_t SpaceFree() const
    {
        return MaxLength - cap;
    }
};

thread_local unsigned KerboStringThreadID = ~0u;
/*
    List of possible string lengths:
    ceil((1u<<n)*m/8)
1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,20,22,24,26,28,30,32,36,40,44,48,52,56,60
64,72,80,88,96,104,112,120,128,144,160,176,192,208,224,240,256,288,320,352,384,416
448,480,512,576,640,704,768,832,896,960,1024,1152,1280,1408,1536,1664,1792,1920,2048
2304,2560,2816,3072,3328,3584,3840,4096,4608,5120,5632,6144,6656,7168,7680,8192,9216
10240,11264,12288,13312,14336,15360,16384,18432,20480,22528,24576,26624,28672,30720
32768,36864,40960,45056,49152,53248,57344,61440,65536,73728,81920,90112,98304,106496
114688,122880,131072

    We could encode length in 8 bits; this leaves 24 bits for start offset + pool index.
    Probably not enough.
*/

class KerboString
{
private:
    static constexpr unsigned LengthBits = 20, PoolIndexBits = 12, PoolOffsetBits = 64 - LengthBits - PoolIndexBits;
    using OnePool  = KerboPool< (1ull << PoolOffsetBits) >;
    using AllPools = std::array<OnePool, (1u<<PoolIndexBits)>;
    std::uint64_t m_data = 0;
public:
    KerboString() {}
    ~KerboString() { clear(); }
    KerboString(KerboString&& b) : m_data(b.m_data) { b.m_data=0; }
    KerboString& operator=(KerboString&& b) { if(&b!=this) { clear(); m_data=b.m_data; b.m_data=0; } return *this; }

  #if 1
    // Google's Sparsehashtable requires this. UGLY UGLY UGLY.
    // It does a copy when it really means a move.
    KerboString(const KerboString& b)
    {
        m_data = b.m_data;
        (const_cast<KerboString&>(b)).m_data = 0;
    }
    KerboString& operator=(const KerboString& b)
    {
        if(this != &b)
        {
            m_data = b.m_data;
            (const_cast<KerboString&>(b)).m_data = 0;
        }
        return *this;
    }
  #endif

    std::size_t size() const
    {
        return m_data % (1u << LengthBits);
    }
    const char* data() const
    {
        unsigned pool_idx = (m_data >> LengthBits) % (1u << PoolIndexBits);
        return GetPools()[pool_idx].GetReadOffset( m_data >> (LengthBits + PoolIndexBits) );
    }
    // Note: c_str() is not implemented.
    void clear()
    {
        unsigned pool_idx = (m_data >> LengthBits) % (1u << PoolIndexBits);
        GetPools()[pool_idx].Free( m_data >> (LengthBits + PoolIndexBits) , size());
        m_data = 0;
        return;
    }
    bool operator==(const KerboString& b) const
    {
        return size() == b.size()
            && std::memcmp(data(), b.data(), size()) == 0;
    }

    explicit KerboString(const std::string& s)
    {
        auto length = s.size();
        assert(length < (1u << LengthBits));

        auto& pools = GetPools();
        unsigned id = KerboStringThreadID;
        assert(id != ~0u);
        for(unsigned pool_index = id; pool_index < (1u << PoolIndexBits); pool_index += 8)
            if(pools[pool_index].SpaceFree() >= length)
            {
                auto& pool = pools[pool_index];
                std::size_t ptr = pool.Allocate(length);
                m_data = (length << 0) + (pool_index << LengthBits) + (ptr << (LengthBits+PoolIndexBits));
                std::memcpy(pool.GetWriteOffset(ptr), s.data(), s.size());
                return;
            }
        throw std::runtime_error("Out of memory");
    }

private:
    AllPools& GetPools() const
    {
        static AllPools p;
        return p;
    }
};

namespace std
{
    template<>
    struct hash<KerboString>
    {
        std::size_t operator() (const KerboString& s) const noexcept
        {
            return std::_Hash_impl::hash(s.data(), s.size());
        }
    };
};
