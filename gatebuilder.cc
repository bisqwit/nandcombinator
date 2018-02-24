#include <algorithm>
#include <cstdint>
#include <vector>
#include <array>
#include <mutex>
#include <map>
#include <cmath>
#include <cassert>
#include <numeric>
#include <bitset>
#include <cstring>

#include <omp.h>

static constexpr unsigned global_max_gates  = 16;
static constexpr unsigned global_max_inputs = 9;
static constexpr unsigned MAX_THREADS = 8;

using state_bitmask_t
    = std::conditional_t< (global_max_gates <= 32),
      std::uint32_t, std::uint64_t>;

using saved_state_bitmask_t
    = std::conditional_t< (global_max_gates <= 32),
      std::conditional_t< (global_max_gates <= 16),
      std::conditional_t< (global_max_gates <=  8),
      std::uint8_t, std::uint16_t>, std::uint32_t>, std::uint64_t>;

using state_with_input_bitmask_t
    = std::conditional_t< (global_max_inputs+global_max_gates <= 32),
      std::conditional_t< (global_max_inputs+global_max_gates <= 16),
      std::conditional_t< (global_max_inputs+global_max_gates <=  8),
      std::uint8_t, std::uint16_t>, std::uint32_t>, std::uint64_t>;

//using gate_input_bitmask_t
//    = std::conditional_t< (global_max_gates*2 <= 32), std::uint32_t, std::uint64_t>;
//using gate_bitmask_t
//    = std::conditional_t< (global_max_gates <= 32), std::uint32_t, std::uint64_t>;

#include <unistd.h>
//#include <sys/fcntl.h>
//#include <sys/stat.h>

//std::mutex io_lock;

thread_local char filebuf[1048576];

static std::FILE* handles[global_max_inputs+1][global_max_gates+1][MAX_THREADS] = {};

/* Return value: Index of first redundant gate (1+), 0 = all good */
static state_bitmask_t Catalogue(const unsigned char* gate_inputs, unsigned num_gates, unsigned num_inputs)
{
    // We now generate gates where gate(n) can never depend on gate(n+1),
    // so there is no need to calculate an evaluation order
    // or to verify for loops.

    saved_state_bitmask_t gate_outputs[1u << global_max_inputs];

    constexpr unsigned max_inputs_rounded_up = (global_max_inputs+7)&~7;
    state_bitmask_t has_zero=0, has_one=0;
    state_bitmask_t allbits = ~0u, all_gatebits = (1u << num_gates)-1;
    state_bitmask_t has_differing_inputs[max_inputs_rounded_up]{};
    //#pragma omp simd
    for(unsigned truth_index = 0; truth_index < (1u << num_inputs); ++truth_index)
    {
        // sources 0..N-1: input bits
        // sources N...:   gate outputs
        state_with_input_bitmask_t tempstate = truth_index;

        for(unsigned n=0; n<num_gates; ++n)
        {
            unsigned gateno = n;//gate_processing_order[n];
            unsigned mask =
                (tempstate >> gate_inputs[gateno*2+0])
              & (tempstate >> gate_inputs[gateno*2+1]);

            mask = ~mask; // Since it’s a NAND, invert the outcome
            // Can't be SIMDed because each gate may depend on the output of the previous one
            tempstate |= (mask&1) << (gateno + num_inputs);
        }

        state_bitmask_t state = tempstate >> num_inputs; // Don't need the input bitmasks anymore
        gate_outputs[truth_index] = state;
        has_one  |= state;
        has_zero |= state^allbits;
        #pragma omp simd
        for(unsigned m=0; m < max_inputs_rounded_up; ++m)
        {
            state_bitmask_t mask = state_bitmask_t((truth_index >> m) & 1) * allbits;
            has_differing_inputs[m] |= (state ^ mask);
        }
    }

    // If there is a gate that always outputs 1, or always outputs 0,
    // or always outputs a copy of one of the inputs,
    // reject this result.
    state_bitmask_t bad_gates = ~(has_zero & has_one);
    if(!(bad_gates & all_gatebits))
    {
        #pragma omp simd
        for(unsigned m=0; m < max_inputs_rounded_up; ++m) bad_gates |= ~has_differing_inputs[m];
    }
    if(bad_gates & all_gatebits) return __builtin_ffsl(bad_gates);

    //for(unsigned m=0; m<num_inputs; ++m)
    //    //std::printf("zero %08X one %08X bad %08X ind=%d\n", has_zero,has_one,bad_gates, __builtin_ffsl(bad_gates));
    //    {unsigned r = __builtin_ffsl(no_differing_inputs[m]); if(r && r <= num_gates) return r;}

    unsigned thr = omp_get_thread_num();
    assert(thr < MAX_THREADS);

    auto& fp = handles[num_inputs][num_gates][thr];
    if(!fp)
    {
        char Buf[64];
        std::sprintf(Buf, "gates-%02uinput-%02ugate-thread%u.dat", num_inputs, num_gates, thr);
        fp = std::fopen(Buf, "wb");
        setbuffer(fp, filebuf, sizeof(filebuf));
    }
    std::fwrite(&gate_inputs[0],  2,                             num_gates,        fp);
    std::fwrite(&gate_outputs[0], sizeof(saved_state_bitmask_t), 1u << num_inputs, fp);
    //std::fflush(fp);
    return 0;
}

static void CreateNANDcombinations(unsigned num_gates, unsigned num_inputs)
{
    auto begin = std::chrono::system_clock::now();

    // Input can be: INPUT#, Gate# output.
    double ncomb = std::pow(num_gates + num_inputs, double(num_gates*2));
    const unsigned num_sources = num_inputs + num_gates;
    std::uint_fast64_t combinations = 1;
    for(unsigned n=0; n<num_gates*2; ++n)
    {
        combinations *= num_sources;
    }

    unsigned length = std::printf("Calculating %u inputs, %u gates... %llu permutations (%.2g)\n",
        num_inputs,
        num_gates,
        (unsigned long long) combinations, ncomb);

    // 5 6 7 8 9   (5 inputs, 5 gates)
    // 
    // gate:  0011223344
    // index: 0123456789  // 2 gates*2
    // max:   4455667788
    #pragma omp parallel for collapse(4) schedule(dynamic)
    for(unsigned first  = 0; first  < num_inputs; ++first)
    for(unsigned second = 0; second < num_inputs; ++second)
    for(unsigned third  = 0; third  < num_inputs+1; ++third)
    for(unsigned fourth = 0; fourth < num_inputs+1; ++fourth)
    {
        if(first > second || third > fourth) reloop:continue;
        if(__builtin_expect(num_gates==1, false))
            {if(third||fourth)continue;}
        else
            {if(!third&&!fourth)continue;}

        std::array<unsigned char, global_max_gates*2> gate_inputs{};
        gate_inputs[0] = first;
        gate_inputs[1] = second;
        gate_inputs[2] = third;
        gate_inputs[3] = fourth;

        {// Adjust the initial values
        std::bitset< (global_max_inputs+global_max_gates)*(global_max_inputs+global_max_gates) > used;
        for(unsigned n=0; n<num_gates; ++n)
        {
        p:; unsigned pair = gate_inputs[n*2+0] + gate_inputs[n*2+1]*num_sources;
            if(used.test(pair)) { if(n<2) goto reloop; ++gate_inputs[n*2+1]; goto p; }
            used.set(pair);
        }}

        /*bool fail=false;
        for(unsigned n=0; n<num_gates; ++n)
            if(gate_inputs[n*2+1] >= num_inputs+n)
                { fail=true; break; }
        if(fail) continue;*/

        for(;;)
        {
            unsigned pos = num_gates*2-1;
            if(false)
            {
            inc_from:
                // Input: pos = Index of first gate-input to be changed
                // Everything after that will be reset
                // This code is here because of scoping issues.
                for(unsigned m=pos; ++m<num_gates*2; ) gate_inputs[m]=0;
                goto inc;
            }

            for(;;)
            {
                std::uint_fast32_t used_inputs = ~std::uint_fast32_t() << num_inputs;
                for(unsigned n=0; n<num_gates*2; ++n)
                    if(gate_inputs[n] < num_inputs)
                        used_inputs |= state_with_input_bitmask_t(1) << gate_inputs[n];
                std::uint_fast32_t unused_inputs = ~used_inputs;
                if(!unused_inputs) break;

                // We can speed up the iteration a bit by skipping
                // iterations that would not remedy the unused-input situation.
                if(pos < 4) goto inc;
                for(;;)
                {
                    unsigned in = ++gate_inputs[pos];
                    if(in < num_inputs)
                    {
                        if(unused_inputs & (1u << in)) break;
                    }
                    else
                    {
                        goto inc_wraps;
                    }
                }
            }

            for(unsigned n=1; n<num_gates; ++n)
            {
                unsigned b1 = gate_inputs[n*2+0], b2 = gate_inputs[n*2+1]; // Later
                if(__builtin_expect(b1 > b2, false)) goto inc; // Verify ordering
                for(unsigned m=0; m<n; ++m)
                {
                    unsigned a1 = gate_inputs[m*2+0], a2 = gate_inputs[m*2+1]; // Earlier
                    if(std::make_pair(a1,a2) > std::make_pair(b1,b2))
                    {
                        // This case is an error IF these two can be swapped
                        bool swap_ok = true;
                        if(a1 >= num_inputs + n) swap_ok = false;
                        if(a2 >= num_inputs + n) swap_ok = false;
                        if(b1 >= num_inputs + m) swap_ok = false;
                        if(b2 >= num_inputs + m) swap_ok = false;
                        if(swap_ok)
                        {
                            pos = std::max(n,m)*2+1; goto inc_from;
                        }
                    }
                }
            }

            //#pragma omp task firstprivate(gate_inputs)
            {
                auto redundant_gate = Catalogue(&gate_inputs[0],num_gates, num_inputs);
                if(redundant_gate)
                {
                    pos = (redundant_gate-1)*2+1; goto inc_from;
                }
            }

    inc:;
            if(pos < 4) break;
            if(++gate_inputs[pos] >= (num_inputs + pos/2))
            {
    inc_wraps:;
                if(pos == 4) break;
                gate_inputs[pos] = 0;
                --pos;
                goto inc;
            }
            if(pos == num_gates*5/7)
            {
                unsigned pos=0, max=0;
                for(unsigned n=0; n<4 && n<num_gates*2; ++n)
                {
                    pos *= (num_inputs+n/2); pos += gate_inputs[n];
                    max *= (num_inputs+n/2); max += (num_inputs+n/2-1);
                }
                std::printf("\r%8.2f%%", pos*100.0/(max+1)); std::fflush(stdout);
            }
            while(pos < num_gates*2-1)
            {
                gate_inputs[pos+1] = ((pos+1)&1) ? gate_inputs[pos] : 0;
                ++pos;
            }
            std::bitset< (global_max_inputs+global_max_gates)*(global_max_inputs+global_max_gates) > used;
            for(unsigned n=0; n<num_gates; ++n)
            {
                unsigned pair = gate_inputs[n*2+0] + gate_inputs[n*2+1]*num_sources;
                if(used.test(pair)) goto inc;
                used.set(pair);
            }
            for(unsigned n=1; n<num_gates; ++n)
            {
                unsigned b1 = gate_inputs[n*2+0], b2 = gate_inputs[n*2+1];
                // If both inputs come from a single gate,
                // and the inputs of _that_ gate also come from a single source (either input or gate),
                // we have a NOT-NOT. Forbid them.
                if(b1 == b2 && b1 >= num_inputs)
                {
                    unsigned m = b1-num_inputs;
                    unsigned a1 = gate_inputs[m*2+0], a2 = gate_inputs[m*2+1];
                    if(a1 == a2) { pos=n*2+1; goto inc_from; }
                }
            }
        }
    }
    //#pragma omp taskwait

    auto end = std::chrono::system_clock::now();
    double duration = std::chrono::duration<double>(end - begin).count();
    std::printf("\r\33[1A\33[%dC, %02d:%.4g minutes elapsed\n", length, int(duration/60), std::fmod(duration,60.0));
}

int main()
{
    std::vector<std::pair<unsigned,unsigned>> com;
    for(unsigned num_inputs=1; num_inputs<=global_max_inputs; ++num_inputs)
    for(unsigned num_gates=1; num_gates<=global_max_gates; ++num_gates)
    {
        if(num_inputs == 1 && num_gates > 1) continue;

        if(num_inputs <= 4 && num_gates <= 9) continue;
        if(num_inputs <= 9 && num_gates <= 7) continue;
        if(num_inputs <= 3 && num_gates <= 10) continue;
        if(num_inputs == 3) continue;
        if(num_inputs == 2 && num_gates > 11) continue;
        if(num_gates <= 6) continue;

        com.emplace_back(num_inputs, num_gates);
    }

    std::sort(com.begin(), com.end(), [](auto a, auto b)
    {
        if(a.second <= (7 + (a.first<5)) || b.second <= (7 + (b.first<5)))
        {
            double a_ncomb = std::pow(a.first + a.second, double(a.second*2));
            double b_ncomb = std::pow(b.first + b.second, double(b.second*2));
            return a_ncomb < b_ncomb;
        }
        return a < b;
    });

    for(auto[num_inputs,num_gates]: com)
    {
        //double ncomb = std::pow(num_inputs + num_gates, double(num_gates*2));
        //if(ncomb < 22876792454961) continue;

        CreateNANDcombinations(num_gates, num_inputs);

        for(auto& fp: handles[num_inputs][num_gates])
            if(fp)
                std::fclose(fp);
    }
}

/*

Special cases:

    NAND(x,0) = 1         -USELESS
    NAND(x,1) = NOT(x)    -REDUNDANT, NAND(x,x) DOES SAME
    NAND(0,0) = 1         -USELESS
    NAND(0,1) = 1         -USELESS
    NAND(1,1) = 0         -USELESS

    So there is no reason whatsoever to provide literal 0/1 inputs to NANDs.
    This also extends to NANDs that always produce 1 or 0.

*/