/*  Hawkeye with Belady's Algorithm Replacement Policy
    Code for Hawkeye configurations of 1 and 2 in Champsim */

#include "../inc/champsim_crc2.h"
#include <map>
#include <math.h>
#include <cassert>

#define NUM_CORE 1
#define LLC_SETS NUM_CORE*2048
#define LLC_WAYS 16

//3-bit RRIP counter
#define MAXRRIP 7
uint32_t rrip[LLC_SETS][LLC_WAYS];

#include "hawkeye_predictor.h"
#include "optgen.h"
#include "helper_function.h"

//Hawkeye predictors for demand and prefetch requests
Hawkeye_Predictor* predictor_demand;    //2K entries, 5-bit counter per each entry

#define OPTGEN_VECTOR_SIZE 128
OPTgen set_optgen[LLC_SETS];   //2048 vecotrs, 128 entries each

std::vector<std::map<uint64_t, ADDR_INFO>> addr_history; // sampler indexed by pAddr % NSETS
uint64_t sample_signature[LLC_SETS][LLC_WAYS];

// some helpers
bool predict(uint64_t PC);
void replace_addr_history_element(unsigned int set_index);
void update_addr_history_lru(unsigned int set_index, unsigned int curr_lru);
uint64_t c_rc(uint64_t address);

//History time
#define TIMER_SIZE 1024
uint64_t set_timer[LLC_SETS];   //64 sets, where 1 timer is used for every set

// Initialize replacement state
void InitReplacementState()
{
    cout << "Initialize Hawkeye replacement policy state" << endl;

    for (int i=0; i<LLC_SETS; i++) {
        for (int j=0; j<LLC_WAYS; j++) {
            rrip[i][j] = 0; // Initialize RRIP counters to 0
            sample_signature[i][j] = 0; // Initialize sample signatures to 0
        }
        set_timer[i] = 0;
        set_optgen[i].init(LLC_WAYS - 2); // WAYS - 2
    }

    addr_history.resize(LLC_SETS);
    predictor_demand = new Hawkeye_Predictor();

    cout << "Finished initializing Hawkeye replacement policy state" << endl;
}

// Find replacement victim
// Return value should be 0 ~ 15 or 16 (bypass)
uint32_t GetVictimInSet (uint32_t cpu, uint32_t set, const BLOCK *current_set, uint64_t PC, uint64_t paddr, uint32_t type)
{
    //Find the line with RRPV of 7 in that set
    for(uint32_t i = 0; i < LLC_WAYS; i++){
        if(rrip[set][i] == MAXRRIP){
            predictor_demand->decrease(sample_signature[set][i]);
            return i;
        }
    }

    //If no RRPV of 7, then we find next highest RRPV value (oldest cache-friendly line)
    uint32_t max_rrpv = 0;
    int32_t victim = -1;
    for(uint32_t i = 0; i < LLC_WAYS; i++){
        if(rrip[set][i] >= max_rrpv){
            max_rrpv = rrip[set][i];
            victim = i;
        }
    }

    //Asserting that LRU victim is not -1
    //Predictor will be trained negaively on evictions
    predictor_demand->decrease(sample_signature[set][victim]);

    return victim;
}

// //Helper function for "UpdateReplacementState" to update cache history
// void update_cache_history(unsigned int sample_set, unsigned int currentVal){
//     for(map<uint64_t, HISTORY>::iterator it = cache_history_sampler[sample_set].begin(); it != cache_history_sampler[sample_set].end(); it++){
//         if((it->second).lru < currentVal){
//             (it->second).lru++;
//         }
//     }

// }

// Called on every cache hit and cache fill
void UpdateReplacementState (uint32_t cpu, uint32_t set, uint32_t way, uint64_t paddr, uint64_t PC, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    paddr = (paddr >> 6) << 6;

    //Ignore all types that are writebacks
    if(type == WRITEBACK){
        return;
    }

    uint64_t set_index = paddr % LLC_SETS;
    uint64_t tag_index = c_rc(paddr >> 12) % OPTGEN_VECTOR_SIZE;

    // line is new
    if (addr_history[set_index].find(tag_index) == addr_history[set_index].end()) {
        uint64_t curr_quanta = set_timer[set_index] % OPTGEN_VECTOR_SIZE;

        if (addr_history[set_index].size() == LLC_WAYS)
            replace_addr_history_element(set_index);
        assert(addr_history[set_index].size() < LLC_WAYS);

        addr_history[set_index][tag_index].init(curr_quanta);
        set_optgen[set_index].add_access(curr_quanta);
        update_addr_history_lru(set_index, LLC_WAYS-1);
    
        addr_history[set_index][tag_index].update(set_timer[set_index], PC, predict(PC));
        addr_history[set_index][tag_index].lru = 0;
        set_timer[set_index] = (set_timer[set_index]+1) % TIMER_SIZE;

    } else {    // line has been used before -> demand
        uint64_t curr_quanta = set_timer[set_index] % OPTGEN_VECTOR_SIZE;
        uint64_t last_quanta = addr_history[set_index][tag_index].last_quanta % OPTGEN_VECTOR_SIZE;
        unsigned int curr_timer = set_timer[set_index];
        if (curr_timer < addr_history[set_index][tag_index].last_quanta)
            curr_timer = curr_timer + TIMER_SIZE;
        bool wrap = ((curr_timer - addr_history[set_index][tag_index].last_quanta) > OPTGEN_VECTOR_SIZE);

        if (!wrap && set_optgen[set_index].should_cache(curr_quanta, last_quanta)) {
            predictor_demand->increase(addr_history[set_index][tag_index].PC);
        } else {
            predictor_demand->decrease(addr_history[set_index][tag_index].PC);
        }

        set_optgen[set_index].add_access(curr_quanta);
        update_addr_history_lru(set_index, addr_history[set_index][tag_index].lru);

        addr_history[set_index][tag_index].update(set_timer[set_index], PC, predict(PC));
        addr_history[set_index][tag_index].lru = 0;
        
        set_timer[set_index] = (set_timer[set_index]+1) % TIMER_SIZE;
    }

    //Retrieve Hawkeye's prediction for line
    bool prediction = predict(PC);
    
    sample_signature[set][way] = PC;
    //Fix RRIP counters with correct RRPVs and age accordingly
    if(!prediction){
        rrip[set][way] = MAXRRIP;
    }
    else{
        rrip[set][way] = 0;
        if(!hit){
            //Verifying RRPV of lines has not saturated
            bool isMaxVal = false;
            for(uint32_t i = 0; i < LLC_WAYS; i++){
                if(rrip[set][i] == MAXRRIP-1){
                    isMaxVal = true;
                }
            }

            //Aging cache-friendly lines that have not saturated
            for(uint32_t i = 0; i < LLC_WAYS; i++){
                if(!isMaxVal && rrip[set][i] < MAXRRIP-1){
                    rrip[set][i]++;
                }
            }
        }
        rrip[set][way] = 0;
    }

}

// Use this function to print out your own stats on every heartbeat 
void PrintStats_Heartbeat()
{
    int hits = 0;
    int access = 0;
    for(int i = 0; i < LLC_SETS; i++){
        hits += set_optgen[i].get_num_opt_hits();
        access += set_optgen[i].demand_access;
    }

    cout<< "OPTGen Hits: " << hits << endl;
    cout<< "OPTGen Access: " << access << endl;
    cout<< "OPTGEN Hit Rate: " << 100 * ( (double)hits/(double)access )<< endl;
}

// Use this function to print out your own stats at the end of simulation
void PrintStats()
{
    int hits = 0;
    int access = 0;
    for(int i = 0; i < LLC_SETS; i++){
        hits += set_optgen[i].get_num_opt_hits();
        access += set_optgen[i].demand_access;
    }

    cout<< "Final OPTGen Hits: " << hits << endl;
    cout<< "Final OPTGen Access: " << access << endl;
    cout<< "Final OPTGEN Hit Rate: " << 100 * ( (double)hits/(double)access )<< endl;

}

// helper functions
bool predict(uint64_t PC) {
    return predictor_demand->get_prediction(PC);
}

void replace_addr_history_element(unsigned int set_index)
{
    uint64_t lru_addr = 0;
    
    for(std::map<uint64_t, ADDR_INFO>::iterator it=addr_history[set_index].begin(); it != addr_history[set_index].end(); it++)
    {
        if((it->second).lru == (LLC_WAYS-1))
        {
            lru_addr = it->first;
            break;
        }
    }
    addr_history[set_index].erase(lru_addr);
}

void update_addr_history_lru(unsigned int set_index, unsigned int curr_lru)
{
    for(std::map<uint64_t, ADDR_INFO>::iterator it=addr_history[set_index].begin(); it != addr_history[set_index].end(); it++)
    {
        if((it->second).lru < curr_lru)
        {
            (it->second).lru++;
            assert((it->second).lru < LLC_WAYS); 
        }
    }
}

uint64_t c_rc(uint64_t address){
    unsigned long long crcPolynomial = 3988292384ULL;  //Decimal value for 0xEDB88320 hex value
    unsigned long long result = address;
    for(unsigned int i = 0; i < 32; i++ )
        if((result & 1 ) == 1 ){
            result = (result >> 1) ^ crcPolynomial;
        }
        else{
            result >>= 1;
        }
    return result;
}