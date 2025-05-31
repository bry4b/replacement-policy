////////////////////////////////////////////
//                                        //
//        DRRIP replacement policy        //
//        Based on SRRIP/BRRIP ideas      //
//                                        //
////////////////////////////////////////////

#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS NUM_CORE*2048
#define LLC_WAYS 16

#define RRPV_MAX 3
#define PSEL_MAX 1023

// DRRIP components
uint32_t rrpv[LLC_SETS][LLC_WAYS]; // RRPV per line
uint32_t psel = PSEL_MAX / 2;      // Policy selector

// Set ranges for SRRIP and BRRIP dedicated sets
#define SDM_SET_MASK 0xF // bit mask 
bool is_dedicated_srrip(uint32_t set) { return (set & SDM_SET_MASK) == 0; }
bool is_dedicated_brrip(uint32_t set) { return (set & SDM_SET_MASK) == 1; }

// initialize replacement state
void InitReplacementState()
{
    cout << "Initialize DRRIP replacement state" << endl;

    for (int i=0; i<LLC_SETS; i++) {
        for (int j=0; j<LLC_WAYS; j++) {
            rrpv[i][j] = RRPV_MAX;
        }
    }

    psel = PSEL_MAX / 2;
}

// find replacement victim
uint32_t GetVictimInSet(uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    while (true) {
        for (int i = 0; i < LLC_WAYS; i++) {
            if (rrpv[set][i] == RRPV_MAX) {
                return i;
            }
        }

        // Increment all RRPVs if no line is ready to evict
        for (int i = 0; i < LLC_WAYS; i++) {
            if (rrpv[set][i] < RRPV_MAX)
                rrpv[set][i]++;
        }
    }
}

// called on every cache hit and cache fill
void UpdateReplacementState(uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    if (hit) {
        // Cache hit: promote by resetting RRPV
        rrpv[set][way] = 0;
    } else {

        // Determine policy for this set
        bool use_brrip;
        if (is_dedicated_srrip(set))
            use_brrip = false;
        else if (is_dedicated_brrip(set))
            use_brrip = true;
        else
            use_brrip = (psel >= (PSEL_MAX / 2));

        // Insert RRPV
        if (use_brrip) {
            // BRRIP
            rrpv[set][way] = (rand() % 32 == 0) ? (RRPV_MAX) : (RRPV_MAX-1);
        } else {
            // SRRIP: insert with max-1
            rrpv[set][way] = (RRPV_MAX-1);
        }


    }

    // Update PSEL if in dedicated set
    if (is_dedicated_srrip(set)) {
        if (hit && psel < PSEL_MAX) psel++;
    } else if (is_dedicated_brrip(set)) {
        if (hit && psel > 0) psel--;
    }

    // cout << "Updated Replacement State: " << "Set: " << set << ", Way: " << way << ", RRPV: " << rrpv[set][way] << ", PSEL: " << psel << endl;

    return;

}

// use this function to print out your own stats on every heartbeat 
void PrintStats_Heartbeat()
{
    cout << "PSEL counter: " << psel << endl;
}

// use this function to print out your own stats at the end of simulation
void PrintStats()
{
    cout << "Final PSEL value: " << psel << endl;
}
