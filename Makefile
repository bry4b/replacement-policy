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


run-test-%: 
	@mkdir -p ./outputs
	@echo "Running graph_analytics_10M trace $* policy..."
	./build/$* -warmup_instructions 1000000 -simulation_instructions 10000000 -traces ./trace/graph_analytics_10M.trace.gz > ./outputs/graph_analytics_10M.trace.gz.$*.out; \

	@echo "Calculating miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_access += $$4; total_miss += $$8 } END { if (total_access > 0) print "Average Miss Rate:", total_miss / total_access }' ./outputs/*.$*.out

run-all-%: 
	@mkdir -p ./outputs
	@echo "Running all traces with policy: $*"
	@for traces in $(TRACES); do \
		echo "Running $$traces with $* policy..."; \
		./build/$* -warmup_instructions 1000000 -simulation_instructions 10000000 -traces ./trace/$$traces > ./outputs/$$traces.$*.out; \
	done

	@echo "Calculating miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_access += $$4; total_miss += $$8 } END { if (total_access > 0) print "Average Miss Rate:", total_miss / total_access }' ./outputs/*.$*.out
	$(MAKE) calc-new-missrate-$*

calc-missrate-%: 
	@echo "Calculating miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_access += $$4; total_miss += $$8 } END { if (total_access > 0) print "Average Miss Rate:", total_miss / total_access }' ./outputs/*.$*.out

calc-new-missrate-%: 
	@echo "Calculating new miss rate for policy: $*"
	@awk '/LLC TOTAL/ { total_mr += $$8/$$4 } END { print "New Average Miss Rate:", total_mr / 4 }' ./outputs/*.$*.out

clean:
	rm -f build *.o

clean-outputs-all:
	rm -f ../outputs/*.out

clean-outputs-%: 
	rm -f ./outputs/*.$*.out
