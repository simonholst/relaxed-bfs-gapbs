#include "xoshiro.h"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>

using namespace XoshiroCpp;

#define N 100'000'000

int main() {
    std::uniform_int_distribution<uint64_t> dist(0, 64);

    uint64_t sum = 0;
    Xoroshiro128PlusPlus xoshiroRng(std::random_device{}());
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        sum += dist(xoshiroRng);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Xoshiro: " << elapsed.count() << " seconds" << std::endl;
    std::cout << sum << std::endl;

    sum = 0;
    std::mt19937 mtRng(std::random_device{}());
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        sum += dist(mtRng);
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Mersenne Twister: " << elapsed.count() << " seconds" << std::endl;
    std::cout << sum << std::endl;

    sum = 0;
    Xoshiro256Plus xoshiro256p(std::random_device{}());
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        sum += dist(xoshiro256p);
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Xoshiro 256+: " << elapsed.count() << " seconds" << std::endl;
    std::cout << sum << std::endl;

    sum = 0;
    Xoroshiro128StarStar xoshiro128ss(std::random_device{}());
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        sum += dist(xoshiro128ss);
    }
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "Xoroshiro 128**: " << elapsed.count() << " seconds" << std::endl;
    std::cout << sum << std::endl;
}