# See LICENSE.txt for license details.

CXX_FLAGS += -std=c++11 -O3 -Wall
PAR_FLAG = -fopenmp
INCLUDE = -I ~/boost_1_82_0 # Change this to your boost path

ifneq (,$(findstring icpc,$(CXX)))
	PAR_FLAG = -openmp
endif

ifneq (,$(findstring sunCC,$(CXX)))
	CXX_FLAGS = -std=c++11 -xO3 -m64 -xtarget=native
	PAR_FLAG = -xopenmp
endif

ifneq ($(SERIAL), 1)
	CXX_FLAGS += $(PAR_FLAG)
endif

KERNELS = bc bfs cc cc_sv pr pr_spmv sssp tc
SUITE = $(KERNELS) converter

.PHONY: all
all: $(SUITE)

% : src/%.cc src/*.h
	mkdir -p bin
	$(CXX) $(CXX_FLAGS) $(INCLUDE) $< -o bin/$@

relax_% : src/relax/%.cc src/*.h src/relax/*.h
	mkdir -p bin
	$(CXX) $(CXX_FLAGS) $(INCLUDE) $< -o bin/$@

# Testing
include test/test.mk

# Benchmark Automation
include benchmark/bench.mk

.PHONY: clean
clean:
	rm -f $(SUITE) test/out/*
	rm -rf bin
