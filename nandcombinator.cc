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
static constexpr unsigned global_max_gates  = 16;
static constexpr unsigned global_max_inputs = 9;

using state_bitmask_t
    = std::conditional_t< (global_max_gates <= 32),
      std::conditional_t< (global_max_gates <= 16),
      std::conditional_t< (global_max_gates <=  8),
      std::uint8_t, std::uint16_t>, std::uint32_t>, std::uint64_t>;
using gate_input_bitmask_t
    = std::conditional_t< (global_max_gates*2 <= 32), std::uint32_t, std::uint64_t>;
using gate_bitmask_t
    = std::conditional_t< (global_max_gates <= 32), std::uint32_t, std::uint64_t>;

static constexpr unsigned min_outputs = 1, global_max_outputs = 5;

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
    if(!k) {return 1;} if(k==1) {return n;}
    auto mloop = [](double a, double b)
    {
        auto result = a;
        for(double t=a+1; t<=b; ++t) result *= t;
        return result;
    };
    return mloop(n-k+1, n) / mloop(2,k);
}

static double CalculateMaxSize(unsigned num_inputs, unsigned num_outputs)
{
    // Number of rows:            1u << num_inputs
    // Number of output patterns: 1u << num_outputs

    // n_output_patterns ^ n_rows
  #if 0
    unsigned bits = (1u << num_inputs);
    double result = std::pow(2.0, bits);
    result -= 2 + num_inputs;
    result = std::pow(result, num_outputs);
  #else
    unsigned bits = (1u << num_inputs);
    double result = std::pow(2.0, bits);
    result -= 2 + num_inputs; // Remove always-0, always-1, and always-duplicate-of-input
    result = binomial(result, num_outputs); // # unique combinations
  #endif

    /* In two input bits + 1 output, there are 16 combinations.
     *          0000 gets removed: output is always 0
     *          1111 gets removed: output is always 1
     *          0101 gets removed: always duplicate of input 0
     *          0011 gets removed: always duplicate of input 1
     *          Number of combinations is 2 + num_inputs.
     * For multiple outputs, it's that (number+1) ^ num_outputs - 1.
     *
     * +1 because it's possible that for _some_ output this didn't match,
     * and -1 to eliminate the case where it didn't match for _any_ output.
     */
    return result;
}

static std::map<unsigned, std::unordered_map<std::string,std::pair<unsigned,std::string>>> Knowledge;

static std::shared_mutex io_lock;
static FILE* logfps[(global_max_inputs+1) * (global_max_outputs+1)] {};

static void SaveOrIgnoreResult(std::unordered_map<std::string,std::pair<unsigned,std::string>>& lore,
                               const unsigned char* gate_inputs,
                               const std::array<unsigned char,global_max_outputs>&                             sys_outputs,
                               const std::array<std::bitset< (1u << global_max_inputs) >, global_max_outputs>& results,
                               unsigned num_gates,
                               unsigned num_inputs,
                               unsigned num_outputs)
{
    std::array<unsigned char,global_max_outputs> output_order{};
    std::iota(&output_order[0], &output_order[0]+num_outputs, 0);

    // Create an output ordering that sorts the truth tables in consistent order
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

    auto make_string = [](auto&& functor) // Like BASE64.
    {
        std::string binary;
        unsigned cache=0, cached_bits=0;
        auto addbits = [&](const unsigned value, unsigned n)
        {
            cache |= value << cached_bits;
            cached_bits += n;
            while(cached_bits >= 6)
            {
                static const char charset[64+1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                binary += charset[cache % (1u << 6)];
                cache       >>= 6;
                cached_bits -= 6;
            }
        };
        functor(addbits);
        if(cached_bits) addbits(0, 6 - (cached_bits % 6));
        return std::move(binary);
    };
    auto key = make_string([&](auto&& addbits)
    {
        for(unsigned n=0; n<num_outputs; ++n)
        {
            const auto& r = results[output_order[n]];
            unsigned bits = 1u << num_inputs;
            for(unsigned pos = 0; pos < bits; ++pos)
                addbits(r.test(pos), 1);
        }
        addbits(num_inputs,  5); // Room for 31 inputs
        addbits(num_outputs, 5); // Room for 31 outputs
    });

    io_lock.lock_shared();
    auto i = lore.find(key);
    if(i != lore.end() && i->second.first <= num_gates)
    {
        io_lock.unlock_shared();
        return;
    }
    io_lock.unlock_shared();
    std::lock_guard<std::shared_mutex> lk(io_lock);

    i = lore.find(key);
    if(i != lore.end() && i->second.first <= num_gates)
    {
        static std::atomic<unsigned> races{};
        std::printf("\33[1;34m  %u race conditions successfully evaded\33[m\r", (unsigned)++races); std::fflush(stdout);
        return;
    }

    auto value = make_string([&](auto&& addbits)
    {
        for(unsigned n=0; n<num_gates*2; ++n) addbits(gate_inputs[n], 6);
        for(unsigned n=0; n<num_outputs; ++n) addbits(sys_outputs[output_order[n]], 5);
    });

    if(__builtin_expect(i == lore.end(), true))
    {
        // Gate inputs: 6 bits: Room for 32 inputs, 32 gates
        // Outputs:     5 bits: Room for 32 gates
        lore.emplace(key, std::make_pair(num_gates, value));
    }
    else
    {
        assert(i->second.first > num_gates);

        // UPDATE
        // Gate inputs: 6 bits: Room for 32 inputs, 32 gates
        // Outputs:     5 bits: Room for 32 gates
        i->second.first  = num_gates;
        i->second.second = value;

        std::printf("\33[1;31mResult updated for %u outputs, %u inputs -- %u gates\33[m\n", num_outputs,num_inputs, num_gates);
    }

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
    std::fprintf(logfp, "%d %s %s\n", num_gates, key.c_str(), value.c_str());
    std::fflush(logfp);
}

static void Catalogue(const unsigned char* gate_inputs,
                      const state_bitmask_t* gate_outputs,
                      unsigned num_gates,
                      unsigned num_inputs)
{
    std::uint_fast64_t unused_gates_mask = ~(~std::uint_fast64_t() << num_gates);
    std::uint_fast64_t unused_inputs_mask = ~(~std::uint_fast64_t() << num_inputs);
    for(unsigned n=0; n<num_gates*2; ++n)
        if(gate_inputs[n] >= num_inputs)
            unused_gates_mask &= ~(std::uint_fast64_t(1) << (gate_inputs[n]-num_inputs));
        else
            unused_inputs_mask &= ~(std::uint_fast64_t(1) << gate_inputs[n]);
    //if(unused_inputs_mask) return;

    const unsigned max_outputs = std::min(num_gates,
                                          std::min(num_inputs + num_gates+2, global_max_outputs));
                                //std::min(global_max_outputs, (unsigned)std::max(1, 5-(int)num_inputs));
    for(unsigned num_outputs=min_outputs; num_outputs <= max_outputs; ++num_outputs)
    {
        auto& lore = Knowledge[(num_inputs << 16) + num_outputs];
        double hyp_size = CalculateMaxSize(num_inputs, num_outputs);

        if(__builtin_popcount(unused_gates_mask) > num_outputs || num_outputs > num_gates)
        {
            /*std::fprintf(stderr, "%u outputs impossible, %u unused gates and %u gates total\n",
                num_outputs, (int)__builtin_popcount(unused_gates_mask), num_gates);*/
            continue;
        }

        std::array<unsigned char,global_max_outputs> sys_outputs{};
        std::iota(sys_outputs.begin(), sys_outputs.begin()+num_outputs, 0);

        while(lore.size() < std::round(hyp_size)) // Next sys_outputs set
        {
            std::uint_fast64_t unused_gates = unused_gates_mask;
            //std::uint_fast64_t unused_inputs = unused_inputs_mask;
            for(unsigned n=0; n<num_outputs; ++n)
                unused_gates &= ~(std::uint_fast64_t(1) << sys_outputs[n]);
            // Note: Unused inputs is not an error.

            if(!unused_gates)
            {
                std::array<std::bitset< (1u << global_max_inputs) >, global_max_outputs> results{};
                for(unsigned truth_index = 0; truth_index < (1u << num_inputs); ++truth_index)
                {
                    state_bitmask_t state = gate_outputs[truth_index];
                    for(unsigned n=0; n<num_outputs; ++n)
                        if((state >> sys_outputs[n]) & 1)
                            results[n].set(truth_index);
                }

#if 0
                unsigned to_be_skipped = 0;
                // Verify that 1) each input affected an output
                //             2) no input bits got translated directly into output
                // DISABLED, BECAUSE ALREADY HANDLED IN GATEBUILDER
                for(unsigned m=0; m<num_outputs; ++m)
                {
                    auto c = results[m].count();
                    // Either always zero or always set?
                    if(c == 0 || c == (1u << num_inputs)) goto skip_to_next_output;
                }
                for(unsigned m=0; m<num_outputs; ++m)
                {
                    unsigned ok_bits = 0;
                    for(unsigned r=0; r < (1u << num_inputs); ++r)
                    {
                        bool bit = results[m].test(r);
                        for(unsigned n=0; n<num_inputs; ++n)
                            if(!(ok_bits & (1u << n)))
                                if(bit != bool(r & (1u << n)))
                                    ok_bits |= 1u << n;
                    }
                    if(ok_bits != (1u << num_inputs)-1)
                        goto skip_to_next_output;
                }
#endif

                SaveOrIgnoreResult(lore,
                                   gate_inputs,sys_outputs,results,
                                   num_gates,num_inputs,num_outputs);
            } // unused gates situation skips here

        //skip_to_next_output:;
            unsigned o = num_outputs-1;
        newo:
            if(++sys_outputs[o] >= num_gates)
            {
                if(!o--) break; // No more sys_outputs sets
                goto newo;
            }
            unsigned where = o;
            while(o+1 < num_outputs)
            {
                sys_outputs[o+1] = sys_outputs[o]+1;
                if(sys_outputs[o+1] >= num_gates) { o=where; goto newo; }
                ++o;
            }
            assert(std::is_sorted(sys_outputs.begin(), sys_outputs.begin()+num_outputs));
            //if(!std::is_sorted(sys_outputs.begin(), sys_outputs.begin()+num_outputs)) { o=num_outputs-1; goto newo; }
        } //Next sys_outputs set
    } // num_outputs
}

static void CreateNANDcombinations(unsigned num_gates, unsigned num_inputs)
{
    std::atomic<unsigned> completed{};

    //const unsigned num_sources = 2 + num_inputs + num_gates;
    auto test_missing = [&]()
    {
        bool missing = false;
        const unsigned max_outputs = std::min(num_gates+4u,
                                              std::min(num_inputs + num_gates+2, global_max_outputs));
        io_lock.lock_shared();
        for(unsigned num_outputs=min_outputs; num_outputs <= max_outputs; ++num_outputs)
        {
            auto& lore = Knowledge[(num_inputs << 16) + num_outputs];
            auto current_size = lore.size();
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
        io_lock.unlock_shared();
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
    std::size_t output_record_size = sizeof(state_bitmask_t) * (1u << num_inputs);
    std::size_t record_size = gate_record_size + output_record_size;

    std::vector<std::tuple<std::atomic<std::size_t>/*record ptr*/,
                           MemMappingType<true>    /*mmap*/,
                           std::size_t             /*num_records*/
                          >> files( globbuf.gl_pathc );

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

        std::fclose(fp);
    }
    globfree(&globbuf);

    std::atomic<std::size_t> next_file = 0;
    std::vector<std::thread> threads;
    std::mutex filenumber_lock;

    for(std::size_t m=std::thread::hardware_concurrency(), n=0; n<m; ++n)
        threads.emplace_back([&,n]
        {
            constexpr unsigned group = 128;

            if(!test_missing()) return;
            unsigned tick=0;
            for(;;)
            {
                if(!n) { std::printf("%c\b", "-\\|/"[++tick%4]); std::fflush(stdout); }

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

                for(std::size_t b=0; b<group; ++b)
                {
                    std::size_t recordnum = record + b;
                    if(recordnum >= num_records) break;

                    unsigned char   gate_inputs[gate_record_size];
                    state_bitmask_t results[1u << global_max_inputs];

                    auto ptr = mmap.get_ptr() + recordnum*record_size;

                    std::memcpy(gate_inputs, ptr+0,                gate_record_size);
                    std::memcpy(results,     ptr+gate_record_size, output_record_size);

                    Catalogue(gate_inputs, results, num_gates,num_inputs);
                }

                if(!test_missing()) break;
            }
        });

    for(auto& t: threads) t.join();
}

int main()
{
    for(unsigned i=0; i<=global_max_inputs; ++i)
        for(unsigned o=min_outputs; o<=global_max_outputs; ++o)
            Knowledge[ (i<<16) + o ];

    std::vector<unsigned> choices;
    for(unsigned num_inputs=1; num_inputs<=global_max_inputs; ++num_inputs)
    for(unsigned num_gates=0; num_gates<=global_max_gates; ++num_gates)
    {
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

    for(unsigned choice: choices)
    {
        unsigned num_inputs  = (choice>>10)%(1<<10);
        unsigned num_gates   = choice%(1<<10);

        std::printf("Begin work with %u gates, %u inputs\n", num_gates, num_inputs);
        CreateNANDcombinations(num_gates, num_inputs);

        for(unsigned o=min_outputs; o<=global_max_outputs; ++o)
        {
            auto current_size = Knowledge[(num_inputs<<16) + o].size();

            double hyp_size = CalculateMaxSize(num_inputs, o);

            std::printf("%7.2f %% (%zu/%g) of choices processed for %u-input %u-outputs (%u gates)\n",
                        current_size*100.0 / hyp_size,
                        current_size, hyp_size,
                        num_inputs,o, num_gates);
        }
    }
}
