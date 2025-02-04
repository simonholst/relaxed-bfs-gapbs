bench_boost_seq: bench_boost.cc
	g++ -O0 -I ~/boost_1_82_0 bench_boost.cc -o bench_boost
	./bench_boost

bench_boost_par: bench_boost.cc
	g++ -O0 -fopenmp -I ~/boost_1_82_0 bench_boost.cc -o bench_boost
	./bench_boost