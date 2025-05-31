////////////////////////////////////////////
//                                        //
//        DIP replacement policy         //
//     Based on LRU + BIP set dueling    //
//                                        //
////////////////////////////////////////////

#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE*2048)
#define LLC_WAYS 16

// DIP parameters
#define NUM_DUEL_SETS 32
#define PSEL_MAX 1023
#define PSEL_INIT (PSEL_MAX / 2)
#define BIP_INSERT_PERCENT 1 

uint32_t lru[LLC_SETS][LLC_WAYS];
uint16_t PSEL = PSEL_INIT;

uint8_t is_lru_set[LLC_SETS] = {0};
uint8_t is_bip_set[LLC_SETS] = {0};

// Hash to pseudo-randomly assign dueling sets
bool is_duel_set(uint32_t set, bool lru)
{
    return (set % (LLC_SETS / NUM_DUEL_SETS) == (lru ? 1 : 2));
}

void InitReplacementState()
{
    std::cout << "Initialize DIP replacement state" << std::endl;

    for (int i = 0; i < LLC_SETS; i++) {
        for (int j = 0; j < LLC_WAYS; j++)
            lru[i][j] = j;

        if (is_duel_set(i, true))
            is_lru_set[i] = 1;
        else if (is_duel_set(i, false))
            is_bip_set[i] = 1;
    }
}

// Victim: LRU
uint32_t GetVictimInSet(uint32_t cpu, uint32_t set, const BLOCK *current_set,
                        uint64_t PC, uint64_t paddr, uint32_t type)
{
    for (int i = 0; i < LLC_WAYS; i++)
        if (lru[set][i] == (LLC_WAYS - 1))
            return i;

    return 0; // fallback
}

void UpdateReplacementState(uint32_t cpu, uint32_t set, uint32_t way,
                            uint64_t paddr, uint64_t PC,
                            uint64_t victim_addr, uint32_t type,
                            uint8_t hit)
{
    // Update PSEL if in dueling set
    if (hit) {
        if (is_lru_set[set] && PSEL < PSEL_MAX)
            PSEL++;
        else if (is_bip_set[set] && PSEL > 0)
            PSEL--;
    }

    // Determine insertion policy
    bool use_lru_insert = false;
    if (is_lru_set[set])
        use_lru_insert = true;
    else if (is_bip_set[set])
        use_lru_insert = false;
    else
        use_lru_insert = (PSEL >= PSEL_MAX / 2);

    bool insert_at_MRU = true;

    if (!use_lru_insert) {
        // BIP: insert at LRU except 1/BIP_INSERT_PERCENT at MRU
        insert_at_MRU = (rand() % BIP_INSERT_PERCENT == 0);
    }

    // LRU promotion: increment others
    for (int i = 0; i < LLC_WAYS; i++) {
        if (lru[set][i] < lru[set][way])
            lru[set][i]++;
    }

    if (insert_at_MRU)
        lru[set][way] = 0;
    else
        lru[set][way] = LLC_WAYS - 1;
}

void PrintStats_Heartbeat() {
    std::cout << "Current PSEL: " << PSEL << std::endl;
    // std::cout << "LUR sets: " << std::count(is_lru_set, is_lru_set + LLC_SETS, 1) << std::endl;
    // std::cout << "BIP sets: " << std::count(is_bip_set, is_bip_set + LLC_SETS, 1) << std::endl;
}
void PrintStats() {
    std::cout << "Final PSEL: " << PSEL << std::endl;
}
