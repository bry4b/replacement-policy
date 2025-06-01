#include "../inc/champsim_crc2.h"
#include <unordered_map>

#define NUM_CORE 1
#define LLC_SETS NUM_CORE * 2048
#define LLC_WAYS 16

#define RRPV_MAX 3
#define SHCT_SIZE 2048  // Can be tuned // 16384 gives 0.508684
#define SHCT_COUNTER_MAX 511

// RRIP counters
uint8_t rrpv[LLC_SETS][LLC_WAYS];

// SHiP state
uint16_t signature[LLC_SETS][LLC_WAYS]; // PC signatures per block
bool reused[LLC_SETS][LLC_WAYS];        // Was the block reused?
uint16_t SHCT[SHCT_SIZE];                // Signature history counter table

// Utility: hash PC into SHCT index
uint32_t get_signature_index(uint64_t PC) {
    uint32_t hash = (uint32_t)(PC ^ (PC >> 32));
    hash ^= (hash >> 16);
    hash ^= (hash >> 8);
    return hash & (SHCT_SIZE - 1);
}

// Init state
void InitReplacementState() {
    cout << "Initialize SHiP Replacement State" << endl;
    
    for (int i = 0; i < LLC_SETS; i++) {
        for (int j = 0; j < LLC_WAYS; j++) {
            rrpv[i][j] = RRPV_MAX;
            signature[i][j] = 0;
            reused[i][j] = false;
        }
    }

    for (int i = 0; i < SHCT_SIZE; i++) {
        SHCT[i] = SHCT_COUNTER_MAX; // Start at midpoint
    }
}

// Victim selection: pick a block with RRPV == MAX
uint32_t GetVictimInSet(uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type) {
    while (true) {
        for (int i = 0; i < LLC_WAYS; i++) {
            if (rrpv[set][i] == RRPV_MAX)
                return i;
        }

        // Increment all RRPVs if none eligible
        for (int i = 0; i < LLC_WAYS; i++)
            if (rrpv[set][i] < RRPV_MAX)
                rrpv[set][i]++;
    }
}

// Update replacement state
void UpdateReplacementState(uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr,
                            uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit) {
    uint32_t sig_idx = get_signature_index(PC);

    // On hit: mark reused
    if (hit) {
        reused[set][way] = true;

        // Update RRPV to MRU (most recently used)
        rrpv[set][way] = 0;
        return;
    }

    // On miss + insert: update SHCT using the reused bit of the evicted block
    if (!reused[set][way]) {
        uint32_t old_sig = signature[set][way];
        if (SHCT[old_sig] > 0)
            SHCT[old_sig]--;
        // else std::cerr << "SHCT underflow: " << old_sig << std::endl;
    } else {
        uint32_t old_sig = signature[set][way];
        if (SHCT[old_sig] < SHCT_COUNTER_MAX)
            SHCT[old_sig]++;
        // else std::cerr << "SHCT overflow: " << old_sig << std::endl;

    }

    // Reset metadata for the new block
    signature[set][way] = sig_idx;
    reused[set][way] = false;

    // Insert with RRPV based on SHCT[signature]
    if (SHCT[sig_idx] <= 0)
        rrpv[set][way] = RRPV_MAX;       // Insert at LRU (low reuse confidence)
    // else if (SHCT[sig_idx] <= 3)
    //     rrpv[set][way] = RRPV_MAX-2;   // Insert at second LRU
    // else if (SHCT[sig_idx] <= 5)
    //     rrpv[set][way] = RRPV_MAX-2;   // Insert at third LRU
    else 
        rrpv[set][way] = 0;     // Insert at MRU
}

void PrintStats_Heartbeat() {}

void PrintStats() {}
