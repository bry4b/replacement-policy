#ifndef HAWKEYE_PREDICTOR_H
#define HAWKEYE_PREDICTOR_H

using namespace std;
#include <vector>
#include <map>
#include <cstdint>
#include "helper_function.h"

#define MAX_PCMAP 31
#define PCMAP_SIZE 2048

class Hawkeye_Predictor{
private:
	std::map<uint64_t, int> PC_Map;
	uint64_t cyclic_rc(uint64_t address){
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
public:
	// Return prediction for PC Address
	bool get_prediction(uint64_t PC) {
		uint64_t result = cyclic_rc(PC) % PCMAP_SIZE;
		if (PC_Map.find(result) != PC_Map.end() && PC_Map[result] < ((MAX_PCMAP + 1) / 2)) {
			return false;
		}
		return true;
	}

	void increase(uint64_t PC) {
		uint64_t result = cyclic_rc(PC) % PCMAP_SIZE;
		if (PC_Map.find(result) == PC_Map.end()) {
			PC_Map[result] = (MAX_PCMAP + 1) / 2;
		}

		if (PC_Map[result] < MAX_PCMAP) {
			PC_Map[result] += 1;
		} else {
			PC_Map[result] = MAX_PCMAP;
		}
	}

	void decrease(uint64_t PC) {
		uint64_t result = cyclic_rc(PC) % PCMAP_SIZE;
		if (PC_Map.find(result) == PC_Map.end()) {
			PC_Map[result] = (MAX_PCMAP + 1) / 2;
		}
		if (PC_Map[result] != 0) {
			PC_Map[result] -= 1;
		}
	}

};

#endif