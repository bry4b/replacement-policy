CXXFLAGS := -Wall -std=c++11
#CXXFLAGS := -g -Wall -lm
CXX=g++
BUILD=./build
TRACES := bzip2_10M.trace.gz graph_analytics_10M.trace.gz libquantum_10M.trace.gz mcf_10M.trace.gz
SIM_POLICY := srrip

build-%: 
	@mkdir -p $(BUILD)
	@echo "Building with policy: $*"
	$(CXX) $(CXXFLAGS) -o ./build/$* ./src/$*.cc ./lib/config1.a	


run-all-%: 
	@mkdir -p ./outputs
	@echo "Running all traces with policy: $*"
	@for traces in $(TRACES); do \
		echo "Running $$traces with $* policy..."; \
		./build/$* -warmup_instructions 1000000 -simulation_instructions 10000000 -traces ./trace/$$traces > ./outputs/$$traces.$*.out; \
	done

	@echo "Calculating miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_access += $$4; total_miss += $$8 } END { if (total_access > 0) print "Average Miss Rate:", total_miss / total_access }' ./outputs/*.$*.out

calc-missrate-%: 
	@echo "Calculating miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_access += $$4; total_miss += $$8 } END { if (total_access > 0) print "Average Miss Rate:", total_miss / total_access }' ./outputs/*.$*.out
	
clean:
	rm -f build *.o

clean-outputs:
	rm -f ../outputs/*.out
