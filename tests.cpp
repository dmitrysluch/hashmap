#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <random>
#include <time.h>
#include "hashmap.h"

void fail(const char* message) {
  std::cerr << "Fail:\n";
  std::cerr << message;
  std::cout << "I want to get WA\n";
  exit(0);
}

namespace internal_tests {
using namespace std;

void stress_remove() {
  mt19937 rnd;
  rnd.seed(time(0));
  const int kNumTests = 1000, kMaxElements = 3000;
  HashMap<int, int> hash_map;
  vector<pair<int, int>> vec;
  std::cerr << "Testing removing elements\n";
  for (int t = 0; t < kNumTests; ++t) {
    for (int i = 0; i < kMaxElements; ++i) {
      vec.emplace_back(rnd(), rnd());
      hash_map[vec.back().first] = vec.back().second;
      for (auto &[key, value] : vec) {
        if (hash_map[key] != value) {
          fail("Some elements can't be accessed after adding an element");
        }
      }
    }
    while (!vec.empty()) {
      int i = rnd() % vec.size();
      auto [key, value] = vec[i];
      hash_map.erase(key);
      if (hash_map.find(key) != hash_map.end()) {
        fail("Element hasn't been removed");
      }
      vec.erase(vec.begin() + i);
      for (auto & [key, value] : vec) {
        if (hash_map[key] != value) {
          fail("Some elements can't be accessed after removing an element");
        }
      }
    }
    std::cerr << t + 1 << " tests out of " << kNumTests << "\n";
  }
}

void run_all() { stress_remove(); }
}  // namespace internal_tests

int main() {
  internal_tests::run_all();
  return 0;
}