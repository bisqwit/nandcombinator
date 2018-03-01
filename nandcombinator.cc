#include <algorithm>
#include <cstdint>
#include <vector>
#include <array>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <unordered_map>
#include <cmath>
#include <cassert>
#include <numeric>
#include <bitset>
#include <thread>
#include <atomic>
#include <cstring>

#include "builtin.hh"

//#define USE_RADIX_TREE
//#define USE_PATRICIA_TRIE
//#define USE_STD_MAP
//#define USE_UNORDERED_MAP
//#define USE_SPARSE_MAP
#define USE_SPARSE_SET

#ifdef USE_RADIX_TREE
 #include "radixtree/RadixTree-Vector.hh"
#endif
#ifdef USE_PATRICIA_TRIE
 //#include <ext/pb_ds/assoc_container.hpp>
#endif
#include <ext/mt_allocator.h>
#include <ext/pool_allocator.h>
#ifdef USE_SPARSE_MAP
 #include "sparsehash/sparse_hash_map"
#endif
#ifdef USE_SPARSE_SET
 #include "sparsehash/sparse_hash_set"
#endif

#include "kerbostring.hh"

static constexpr unsigned global_max_gates  = 16;
static constexpr unsigned global_max_inputs = 9;

using state_bitmask_t
    = std::conditional_t< (global_max_gates <= 32),
      std::uint32_t, std::uint64_t>;

using saved_state_bitmask_t
    = std::conditional_t< (global_max_gates <= 32),
      std::conditional_t< (global_max_gates <= 16),
      std::conditional_t< (global_max_gates <=  8),
      std::uint8_t, std::uint16_t>, std::uint32_t>, std::uint64_t>;

//using result_bitmask_t
//    = std::conditional_t< (global_max_gates*2 <= 32), std::uint32_t, std::uint64_t>;

//using gate_bitmask_t
//    = std::conditional_t< (global_max_gates <= 32), std::uint32_t, std::uint64_t>;

static constexpr unsigned min_outputs = 1, global_max_outputs = 16;

static constexpr unsigned log2floor(unsigned n) { return n<2 ? 0 : (1+log2floor((n+1)/2)); }
static constexpr unsigned log2ceil(unsigned n)  { return log2floor(n) + !(n & (n-1));      }

static constexpr unsigned minimal_input_bits  = log2ceil(global_max_inputs);
static constexpr unsigned minimal_output_bits = log2ceil(global_max_outputs);
static constexpr unsigned minimal_gate_bits   = log2ceil(global_max_gates);
static constexpr unsigned minimal_source_bits = log2ceil(global_max_gates + global_max_inputs);

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <glob.h>
#include "mmapping.hh"

static std::uint_fast64_t binomial(std::uint_fast64_t n, std::uint_fast64_t k)
{
    k = std::min(k, n-k);
    if(!k) {return 1;} if(k==1) {return n;}
    auto mloop = [](std::uint_fast64_t a, std::uint_fast64_t b)
    {
        auto result = a;
        for(std::uint_fast64_t t=a+1; t<=b; ++t) result *= t;
        return result;
    };
    return mloop(n-k+1, n) / mloop(2,k);
}
static double binomial(double n, double k)
{
    k = std::min(k, n-k);
    if(k < 0) return 0;
    if(!k) {return 1;} if(k==1) {return n;}
    if(k > 1000 || n > 1000)
    {
        return std::tgamma(n+1) / (std::tgamma(n-k + 1)
                                 * std::tgamma(k+1));
    }
    else
    {
        auto mloop = [](double a, double b)
        {
            auto result = a;
            for(double t=a+1; t<=b; ++t) result *= t;
            return result;
        };
        return mloop(n-k+1, n) / std::tgamma(k+1);
    }
}

static double MaxSize[(global_max_inputs+1) * (global_max_outputs+1) + global_max_outputs ]{};
static double CalculateMaxSize(unsigned num_inputs, unsigned num_outputs)
{
    auto& r = MaxSize[num_inputs*(global_max_outputs+1)+num_outputs];
    if(likely(r)) return r;

    // Number of rows:            1u << num_inputs
    // Number of output patterns: 1u << num_outputs

    // n_output_patterns ^ n_rows
    unsigned bits = (1u << num_inputs);
    double result = std::pow(2.0, bits);
    result -= 2 + num_inputs; // Remove always-0, always-1, and always-duplicate-of-input
    if(num_inputs < 6)
        result = binomial(result, num_outputs); // # unique combinations
    else
        result = std::pow(result, num_outputs);
    if(!std::isfinite(result) || (result < 0 || result > 1e200))
        result = 1e200;
    r = result;
    return result;
}

/*
    Max length on 16 inputs, 16 outputs: 1048576 bits -> 131072 bytes (17 bits)
    20 bits for length
    44 bits remain for offset (=167 TB)
    divide into pools of 4 GB (=32 bits)
    12 bits remain for pool index = 4096 pools
*/
#ifdef USE_RADIX_TREE
constexpr unsigned RadixKeySize = ((1u << global_max_inputs) * global_max_outputs+7)/8;
typedef RadixTree<RadixKeySize, 1, 8> knowledge_tree;
#endif
#if defined(USE_SPARSE_MAP) || defined(USE_SPARSE_SET)
typedef KerboString knowledge_string;
#else
typedef std::basic_string<char,std::char_traits<char>,
                          __gnu_cxx::__pool_alloc<char>> knowledge_string;
struct FastStringCompare
{
    bool operator() (const knowledge_string& a, const knowledge_string& b) const
    {
        if(a.size() != b.size()) return a.size() < b.size();
        return std::memcmp(a.data(), b.data(), a.size()) < 0;
    }
};
#endif
#ifdef USE_PATRICIA_TREE
typedef __gnu_pbds::trie<knowledge_string,unsigned char, FastStringCompare> knowledge_tree;
#endif
#ifdef USE_STD_MAP
typedef std::map<knowledge_string,unsigned char, FastStringCompare,
                 __gnu_cxx::__mt_alloc<int>> knowledge_tree;
#endif
#ifdef USE_UNORDERED_MAP
typedef std::unordered_map<knowledge_string,unsigned char, FastStringCompare> knowledge_tree;
#endif
#ifdef USE_SPARSE_MAP
typedef google::sparse_hash_map<knowledge_string,unsigned char, std::hash<knowledge_string>> knowledge_tree;
#endif
#ifdef USE_SPARSE_SET
typedef std::array<google::sparse_hash_set<knowledge_string, std::hash<knowledge_string>>,
                   global_max_gates> knowledge_tree;
#endif

static std::map<unsigned, knowledge_tree> Knowledge;

static std::shared_mutex io_lock;
static FILE* logfps[(global_max_inputs+1) * (global_max_outputs+1)] {};

static std::map<std::pair<unsigned/*num gates*/,unsigned/*num outputs*/>,
                std::vector<std::pair<state_bitmask_t, std::vector<unsigned char>>>> Combinations;

#ifdef USE_SPARSE_SET
static std::size_t LoreSize(const knowledge_tree& lore)
{
    std::size_t result = 0;
    for(auto& l: lore) result += l.size();
    return result;
}
#else
static std::size_t LoreSize(const knowledge_tree& lore)
{
    return lore.size();
}
#endif

static void BuildCombinations(unsigned num_gates)
{
    for(unsigned num_outputs = min_outputs; num_outputs <= global_max_outputs; ++num_outputs)
    {
        // All outputs must be fed by a gate. No two outputs can feed from the same gate.
        // This means that the number of outputs cannot exceed the number of gates.
        if(num_outputs > num_gates)
        {
            continue;
        }
        auto& target = Combinations[std::make_pair(num_gates,num_outputs)];

        std::vector<unsigned char> sys_outputs(num_outputs);
        std::iota(sys_outputs.begin(), sys_outputs.end(), 0);

        for(;;)
        {
            state_bitmask_t bitmask = 0;
            for(auto n: sys_outputs) bitmask |= 1u << n;
            target.emplace_back(std::make_pair(bitmask, sys_outputs));

            unsigned o = num_outputs-1;
        newo:
            unsigned maxvalue = num_gates - ((num_outputs-1) - o);
            if(++sys_outputs[o] >= maxvalue)
            {
                if(!o--) break; // No more sys_outputs sets
                goto newo;
            }
            //unsigned where = o;
            while(o+1 < num_outputs)
            {
                sys_outputs[o+1] = sys_outputs[o]+1;
                //assert(sys_outputs[o+1] < num_gates);
                ++o;
            }
            assert(std::is_sorted(sys_outputs.begin(), sys_outputs.begin()+num_outputs));
        } //Next sys_outputs set

        std::printf("%u gates, %u outputs: %zu combinations\n", num_gates,num_outputs, target.size());
    } // num_outputs
}

template<std::size_t Words>
static void SaveOrIgnoreResult(knowledge_tree& lore,
                               const unsigned char* gate_inputs,
                               const unsigned char* sys_outputs,
                               //const std::array<std::bitset< (1u << global_max_inputs) >, global_max_outputs>& results,
                               const std::array<std::array<std::uint32_t,Words>, global_max_outputs>& results,
                               unsigned num_gates,
                               unsigned num_inputs,
                               unsigned num_outputs)
{
    std::array<unsigned char,global_max_outputs> output_order{};
    std::iota(&output_order[0], &output_order[0]+num_outputs, 0);

    // Create an output ordering that sorts the truth tables in consistent order
#if 0
    std::sort(output_order.begin(), output_order.begin() + num_outputs,
        [&](unsigned index1, unsigned index2)
        {
            return std::memcmp(&results[index1], &results[index2], sizeof(results[0])) < 0;
        });

    for(unsigned n=1; n<num_outputs; ++n)
        if(std::memcmp(&results[output_order[n]], &results[output_order[n-1]], sizeof(results[0])) == 0)
        {
            // Disallow identical outputs
            return;
        }
#else
    const unsigned words = ((1u << num_inputs) + 31u) / 32u;
    for(auto begin = output_order.begin(), end = begin + num_outputs,
             i = std::next(begin); i != end; ++i)
    {
        auto tmp = std::move(*i);
        auto j   = i;
        for(; j != begin; --j)
        {
            int comp = 0;
            const auto& a = results[*std::prev(j)];
            const auto& b = results[tmp];
            for(unsigned w=words; w-- > 0; ) { comp = a[w] - b[w]; if(comp) break; }

            //int comp = std::memcmp(&a, &b, words*sizeof(std::uint32_t));
            // While sorting, check for duplicate truth-tables. Disallow identical outputs.
            if(!comp) return;
            if(comp > 0) break;
            *j = std::move(*std::prev(j));
        }
        *j = std::move(tmp);
    }
    //for(unsigned n=1; n<num_outputs; ++n)
    //    assert(std::memcmp(&results[output_order[n]], &results[output_order[n-1]], sizeof(results[0])) < 0);
#endif

    auto make_string = [](auto&& functor, unsigned outbits) // Like BASE64.
    {
    #if defined(USE_SPARSE_MAP) || defined(USE_SPARSE_SET)
        std::string binary;
    #else
        knowledge_string binary;
    #endif
        unsigned cache=0, cached_bits=0;
        auto addbits = [&](const unsigned value, unsigned n)
        {
            cache |= value << cached_bits;
            cached_bits += n;
            while(cached_bits >= outbits)
            {
                if(likely(outbits == 8))
                {
                    binary += char(cache & 0xFF);
                }
                else
                {
                    static const char charset[64+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                    binary += charset[cache % (1u << outbits)];
                }
                cache       >>= outbits;
                cached_bits -= outbits;
            }
        };
        functor(addbits, outbits);
        if(cached_bits) addbits(0, outbits - (cached_bits % outbits));
        return std::move(binary);
    };
    auto keymaker = [&](auto&& addbits, unsigned outbits)
    {
        const unsigned bits = 1u << num_inputs;
        if(unlikely(outbits == 6))
        {
            addbits(num_inputs,  5); // Room for 31 inputs
            addbits(num_outputs, 5); // Room for 31 outputs
            for(unsigned n=0; n<num_outputs; ++n)
            {
                const auto& r = results[output_order[n]];
                for(unsigned pos = 0; pos < bits; ++pos)
                {
                    addbits( (r[pos/32u] >> (pos%32)) & 1, 1 );
                    //addbits(r.test(pos), 1);
                }
            }
        }
        else
        {
            if(bits >= 32u)
                for(unsigned n=0; n<num_outputs; ++n)
                {
                    const auto& r = results[output_order[n]];
                    for(unsigned pos = 0; pos+32 <= bits; pos += 32)
                        addbits(r[pos/32u], 32u);
                }
            if(bits % 32u)
                for(unsigned n=0; n<num_outputs; ++n)
                    addbits(results[output_order[n]][bits/32u], bits%32u);
        }
    };
#ifdef USE_RADIX_TREE
    std::array<unsigned char, RadixKeySize> search_key{};
    unsigned char                           search_result[1];
    {auto k = make_string(keymaker, 8);
     //std::fprintf(stderr, "Key size %u, %u\n", unsigned(k.size()), RadixKeySize);
     assert(k.size() <= RadixKeySize);
     for(unsigned a=0; a<k.size(); ++a) search_key[(RadixKeySize-1)-a] = k[a];
     //std::copy_n(k.begin(), RadixKeySize, &search_key[0]);
    }
#elif defined(USE_SPARSE_MAP) || defined(USE_SPARSE_SET)
    auto search_key = KerboString(make_string(keymaker, 8));
#else
    auto search_key = make_string(keymaker, 8);
#endif

    {
        std::shared_lock<std::shared_mutex> lk(io_lock);
#ifdef USE_RADIX_TREE
        if(lore.find(search_key, search_result) && search_result[0] <= num_gates)
        {
            return;
        }
#elif defined(USE_SPARSE_SET)
        for(unsigned n=1; n<=global_max_gates; ++n)
        {
            auto i = lore[n-1].find(search_key);
            if(i != lore[n-1].end())
            {
                if(n <= num_gates) return;
                break;
            }
        }
#else
        typename knowledge_tree::iterator i = lore.find(search_key);
        if(i != lore.end() && i->second <= num_gates)
        {
            return;
        }
#endif
    } // end scope for shared_lock

    std::lock_guard<std::shared_mutex> lk(io_lock);

#ifdef USE_RADIX_TREE
    if(lore.find(search_key, search_result))
    {
        if(search_result[0] <= num_gates)
        {
            static std::atomic<unsigned> races{};
            std::printf("\33[1;34m\t   %u race conditions successfully evaded\33[m\r", (unsigned)++races); std::fflush(stdout);
            return;
        }

        // UPDATE
        search_result[0] = num_gates;
        lore.ensure(search_key, search_result);

        std::printf("\33[1;31mResult updated for %u outputs, %u inputs -- %u gates\33[m\n", num_outputs,num_inputs, num_gates);
    }
    else
    {
        search_result[0] = num_gates;
        lore.add(search_key, search_result);
    }
#elif defined(USE_SPARSE_SET)
    for(unsigned n=1; n<=global_max_gates; ++n)
    {
        auto i = lore[n-1].find(search_key);
        if(i != lore[n-1].end())
        {
            if(n <= num_gates)
            {
                static std::atomic<unsigned> races{};
                std::printf("\33[1;34m\t   %u race conditions successfully evaded\33[m\r", (unsigned)++races); std::fflush(stdout);
                return;
            }
            lore[n-1].erase(i);
            std::printf("\33[1;31mResult updated for %u outputs, %u inputs -- %u gates\33[m\n", num_outputs,num_inputs, num_gates);
            break;
        }
    }
    lore[num_gates-1].insert(std::move(search_key));
#else
    typename knowledge_tree::iterator i = lore.find(search_key);
    if(i != lore.end() && i->second <= num_gates)
    {
        static std::atomic<unsigned> races{};
        std::printf("\33[1;34m\t   %u race conditions successfully evaded\33[m\r", (unsigned)++races); std::fflush(stdout);
        return;
    }

    if(likely(i == lore.end()))
    {
    #ifdef USE_PATRICIA_TRIE
        lore.insert(std::make_pair(std::move(search_key), num_gates));
    #elif defined(USE_SPARSE_MAP)
        lore.insert(std::make_pair(std::move(search_key), num_gates));
    #else
        lore.emplace(std::move(search_key), num_gates);
    #endif
    }
    else
    {
        assert(i->second > num_gates);

        // UPDATE
        i->second = num_gates;

        std::printf("\33[1;31mResult updated for %u outputs, %u inputs -- %u gates\33[m\n", num_outputs,num_inputs, num_gates);
    }
#endif

    /*
    std::printf("Result generated for %u inputs, %u outputs -- %u gates\n", num_inputs,num_outputs, num_gates);
    for(unsigned n=0; n<num_gates; ++n)
    {
        std::printf("\t\t");
        { unsigned a=gate_inputs[n*2+0]; if(a < num_inputs) std::printf("in%u",a); else std::printf("g%u.out", a-num_inputs); }
        std::printf("->g%u.in1,  ", n);
        { unsigned a=gate_inputs[n*2+1]; if(a < num_inputs) std::printf("in%u",a); else std::printf("g%u.out", a-num_inputs); }
        std::printf("->g%u.in2,\n", n);
    }
    for(unsigned n=0; n<num_outputs; ++n)
    {
        std::printf("\t\t");
        { unsigned a=sys_outputs[output_order[n]]; std::printf("g%u.out", a); }
        std::printf("->out%u,\n", n);
    }
    std::printf("\n");
    std::printf("\tTruth table:\n");
    for(unsigned n=0; n< (1u << num_inputs); ++n)
    {
        std::printf("\t\t");
        for(unsigned m=0; m<num_inputs; ++m)
            std::printf("%d", int((n >> m)&1));
        std::printf(" ");
        for(unsigned m=0; m<num_outputs; ++m)
            std::printf("%d", (int)results[output_order[m]].test(n));
        std::printf("\n");
    }
    */

    FILE* &logfp = logfps[num_inputs * (global_max_outputs+1) + num_outputs];
    if(!logfp)
    {
        std::string f = "datalog_"+std::to_string(num_inputs) + "_" + std::to_string(num_outputs)+".dat";
        logfp = std::fopen(f.c_str(), "w");
    }

    auto valuemaker = [&](auto&& addbits, unsigned /*outbits*/)
    {
        // Gate inputs: 6 bits: Room for 32 inputs, 32 gates
        // Outputs:     5 bits: Room for 32 gates
        for(unsigned n=0; n<num_gates*2; ++n) addbits(gate_inputs[n],               6);
        for(unsigned n=0; n<num_outputs; ++n) addbits(sys_outputs[output_order[n]], 5);
    };

    auto key   = make_string(keymaker, 6);
    auto value = make_string(valuemaker, 6);
    std::fprintf(logfp, "%d %s %s\n", num_gates, key.c_str(), value.c_str());
    std::fflush(logfp);
}

static void Catalogue(const unsigned char* gate_inputs,
                      const state_bitmask_t* gate_outputs,
                      unsigned num_gates,
                      unsigned num_inputs)
{
    state_bitmask_t used_gates=0;
    for(unsigned n=0; n<num_gates*2; ++n)
        if(gate_inputs[n] >= num_inputs)
            used_gates |= 1u << (gate_inputs[n]-num_inputs);

    for(unsigned num_outputs = min_outputs; num_outputs <= global_max_outputs; ++num_outputs)
    {
        auto& lore = Knowledge[(num_inputs << 16) + num_outputs];
        double hyp_size = CalculateMaxSize(num_inputs, num_outputs);

        // All outputs must be fed by a gate. No two outputs can feed from the same gate.
        // This means that the number of outputs cannot exceed the number of gates.
        if(num_outputs > num_gates)
        {
            continue;
        }
        // All gates must be used for something.
        // If there are more unused gates than outputs, we cannot satisfy the condition.
        if(num_gates - PopulationCount(used_gates) > num_outputs)
        {
            continue;
        }

        for(const auto& [state_used_gates,sys_outputs]: Combinations.find(std::make_pair(num_gates,num_outputs))->second)
        {
            if(LoreSize(lore) >= std::round(hyp_size)) break;
            if(PopulationCount(used_gates | state_used_gates) != num_gates) continue;

        #if 0
            // Note: For 9 inputs, this is not a 9-bit set. This is a 2^9 = 512-bit set.
            //       For 5 inputs, it is a 2^5 = 32 bit set.
            std::array<std::bitset< (1u << global_max_inputs) >, global_max_outputs> results{};
            for(unsigned truth_index = 0; truth_index < (1u << num_inputs); ++truth_index)
            {
                state_bitmask_t state = gate_outputs[truth_index];
                for(unsigned n=0; n<num_outputs; ++n)
                    if(state & (state_bitmask_t(1) << sys_outputs[n]))
                        results[n].set(truth_index);
            }
        #else
            constexpr unsigned wordbitsm = 32, bitsm = (1u<<global_max_inputs), wordsm = (bitsm+wordbitsm-1)/wordbitsm;
            std::array< std::array<std::uint32_t, wordsm>, global_max_outputs> results{};
            unsigned bits = 1u << num_inputs, words=(bits+wordbitsm-1)/wordbitsm;
            assert(num_outputs < global_max_outputs);
            assert(words       <= wordsm);
            for(unsigned n=0; n<num_outputs; ++n)
            {
                unsigned pos = sys_outputs[n];
                for(unsigned word=0; word<words; ++word)
                {
                    std::uint32_t w=0;
                    const auto* src = assume_aligned(&gate_outputs[word * wordbitsm], 32);
                    #pragma omp simd reduction(|:w)
                    for(unsigned bit=0; bit<wordbitsm; ++bit)
                        w |= ((src[bit] >> pos) & 1u) << bit;
                    results[n][word] = w;
                }
            }
        #endif

            // We only take inputs from NAND gate outputs.
            // Gatebuilder already makes sure that no NAND gate produces
            // a constant zero, constant one, or a duplicate of any input,
            // so there is no need to check for that situation.

            SaveOrIgnoreResult(lore,
                               gate_inputs,&sys_outputs[0],results,
                               num_gates,num_inputs,num_outputs);
        }
    } // num_outputs
}

static void CreateNANDcombinations(unsigned num_gates, unsigned num_inputs)
{
    std::atomic<unsigned> completed{};

    //const unsigned num_sources = 2 + num_inputs + num_gates;
    auto test_missing = [&]()
    {
        bool missing = false;
        std::shared_lock<std::shared_mutex> lk(io_lock);
        for(unsigned num_outputs=min_outputs; num_outputs <= global_max_outputs; ++num_outputs)
        {
            auto& lore = Knowledge[(num_inputs << 16) + num_outputs];
            auto current_size = LoreSize(lore);
            double hyp_size = CalculateMaxSize(num_inputs, num_outputs);
            if(current_size >= std::round(hyp_size))
            {
                if(!(completed & (1u << num_outputs)))
                {
                    std::printf("\tCompleted: %u inputs, %u outputs (%g)\n", num_inputs, num_outputs, hyp_size);
                    std::fflush(stdout);
                    completed |= 1u << num_outputs;
                }
                continue;
            }
            missing = true;
        }
        return missing;
    };
    if(!test_missing())
    {
        //std::printf("\tSkipping: %u inputs for %u gates is complete\n", num_inputs,num_gates);
        return;
    }

    char Buf[64];
    std::sprintf(Buf, "gates-%02uinput-%02ugate-*.dat", num_inputs, num_gates);

    glob_t globbuf{};
    glob(Buf, GLOB_NOSORT, nullptr, &globbuf);
    if(!globbuf.gl_pathc && test_missing())
    {
        std::printf("\tWork on %u inputs is INCOMPLETE, no data for %u gates\n", num_inputs, num_gates);
    }

    std::size_t gate_record_size   = num_gates*2;
    std::size_t output_record_size = sizeof(saved_state_bitmask_t) * (1u << num_inputs);
    std::size_t record_size        = gate_record_size + output_record_size;

    std::vector<std::tuple<std::atomic<std::size_t>/*record ptr*/,
                           MemMappingType<true>    /*mmap*/,
                           std::size_t             /*num_records*/
                          >> files( globbuf.gl_pathc );

    std::size_t total_records = 0;
    for(std::size_t index=0; ; ++index)
    {
        if(!test_missing())
        {
            std::printf("\tWork on %u inputs is complete, skipping work on %u gates\n", num_inputs, num_gates);
            break;
        }
        if(index == globbuf.gl_pathc) break;

        std::FILE* fp = fopen(globbuf.gl_pathv[index], "rb");
        if(!fp)
        {
            std::perror(Buf);
            continue;
        }
        std::fseek(fp, 0, SEEK_END);
        std::size_t file_size = std::ftell(fp);
        std::size_t num_records = file_size / record_size;
        std::rewind(fp);
        //std::printf("READING %s - %zu records of %zu bytes\n", globbuf.gl_pathv[index], num_records, record_size);

        std::get<1>(files[index]).SetMap(fileno(fp), 0, file_size, false);
        std::get<2>(files[index]) = num_records;

        total_records += num_records;

        std::fclose(fp);
    }
    globfree(&globbuf);

    std::atomic<std::size_t> next_file = 0;
    std::vector<std::thread> threads;
    std::mutex filenumber_lock;
    std::atomic<std::size_t> records_processed = 0;

    for(std::size_t m=std::thread::hardware_concurrency(), n=0; n<m; ++n)
        threads.emplace_back([&,n]
        {
            KerboStringThreadID = n; // kerbostring.hh

            constexpr unsigned group = 128;

            if(!test_missing()) return;
            unsigned tick=0;
            for(;;)
            {
                if(!n) { std::printf("%c %5.2f%%\b\b\b\b\b\b\b\b",
                                     "-\\|/"[++tick%4],
                                     records_processed*100.0/total_records
                                    ); std::fflush(stdout); }

                std::size_t filenum = next_file;
                if(filenum >= files.size()) break;
                auto num_records  = std::get<2>(files[filenum]);
                auto& next_record = std::get<0>(files[filenum]);
                auto record      = next_record++ * group;
                if(record >= num_records)
                {
                    if(!filenumber_lock.try_lock())
                    {
                        std::this_thread::yield();
                        continue;
                    }
                    if(next_file == filenum) ++next_file;
                    filenumber_lock.unlock();
                    continue;
                }

                auto& mmap = std::get<1>(files[filenum]);
#if 0
                unsigned pmul = 32;
                auto paddr = std::uint_fast64_t( mmap.get_ptr() + record*record_size ) / MMAP_PAGESIZE;
                if(!(paddr % pmul))
                {
                    /*
                    madvise( (void*) ((paddr-pmul) * MMAP_PAGESIZE ),
                             ( group*record_size + MMAP_PAGESIZE*pmul-1) & ~(MMAP_PAGESIZE*pmul-1),
                             MADV_DONTNEED );
                    */

                    madvise( (void*) (paddr * MMAP_PAGESIZE ),
                             ( group*record_size + MMAP_PAGESIZE*pmul-1) & ~(MMAP_PAGESIZE*pmul-1),
                             MADV_SEQUENTIAL | MADV_WILLNEED );
                }
#endif
                for(std::size_t b=0; b<group; ++b)
                {
                    std::size_t recordnum = record + b;
                    if(recordnum >= num_records)
                    {
                        records_processed += b;
                        goto breaki;
                    }

                    unsigned char         gate_inputs[gate_record_size];
                    saved_state_bitmask_t results[1u << global_max_inputs]{};
                    state_bitmask_t       outputs[((1u << global_max_inputs)+31)/32*32] {};

                    auto ptr = mmap.get_ptr() + recordnum*record_size;

                    std::memcpy(gate_inputs, ptr+0,                gate_record_size);
                    std::memcpy(results,     ptr+gate_record_size, output_record_size);

                    #pragma omp simd
                    for(unsigned n=0; n < (1u << global_max_inputs); ++n)
                        outputs[n] = results[n];

                    Catalogue(gate_inputs, outputs, num_gates,num_inputs);
                }
                records_processed += group;
            breaki:;

                if(!test_missing()) break;
            }
        });

    for(auto& t: threads) t.join();
}

int main()
{
    for(unsigned i=0; i<=global_max_inputs; ++i)
        for(unsigned o=min_outputs; o<=global_max_outputs; ++o)
        {
            Knowledge[ (i<<16) + o ];
            CalculateMaxSize(i,o); // Prefill MaxSize
#ifdef USE_SPARSE_SET
            for(auto& k: Knowledge[ (i<<16) + o ])
                k.set_deleted_key( KerboString{} );
#endif
        }

    std::vector<unsigned> choices;
    for(unsigned num_inputs=1; num_inputs<=global_max_inputs; ++num_inputs)
    for(unsigned num_gates=1; num_gates<=global_max_gates; ++num_gates)
    {
        if(num_inputs != 2) continue;
        //if(num_gates>5)continue;
        //if(num_inputs<6)continue;
        choices.push_back( (num_inputs<<10) + (num_gates<<0) );
    }

    std::sort(choices.begin(), choices.end(), [](unsigned a,unsigned b)
    {
        unsigned a2 = (a>>10)%(1<<10), a3 = a%(1<<10);
        unsigned b2 = (b>>10)%(1<<10), b3 = b%(1<<10);
        /*double opt1 = std::pow(double( a2 + a3), double(a3));
        double opt2 = std::pow(double( b2 + b3), double(b3));
        if(opt1 == opt2)
        {
            return (a2*a2 + a3*a3)
                 < (b2*b2 + b3*b3);
        }
        return opt1 < opt2;
        */
        return a2 != b2 ? a2 < b2 : a3 < b3;
    });

    for(unsigned num_gates=1; num_gates<=global_max_gates; ++num_gates)
    {
        BuildCombinations(num_gates);
    }

    for(unsigned choice: choices)
    {
        unsigned num_inputs  = (choice>>10)%(1<<10);
        unsigned num_gates   = choice%(1<<10);

        std::printf("Begin work with %u gates, %u inputs\n", num_gates, num_inputs);
        CreateNANDcombinations(num_gates, num_inputs);

        for(unsigned o=min_outputs; o<=global_max_outputs; ++o)
        {
            auto current_size = LoreSize(Knowledge[(num_inputs<<16) + o]);

            double hyp_size = CalculateMaxSize(num_inputs, o);

            std::printf("%7.2f %% (%zu/%g) of choices processed for %u-input %u-outputs (%u gates)\n",
                        current_size*100.0 / hyp_size,
                        current_size, hyp_size,
                        num_inputs,o, num_gates);
        }
    }
}
