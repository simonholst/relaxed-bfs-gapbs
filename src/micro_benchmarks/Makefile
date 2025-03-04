CXX_FLAGS += -std=c++17 -O3 -Wall

% : %.cc *.h
	mkdir -p bin
	$(CXX) $(CXX_FLAGS) $< -o bin/$@

clean:
	rm -rf bin