#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <yeptris/tape.h>

int main(int argc, char **argv) {
  const int entries = argc > 1 ? atoi(argv[1]) : 30000;
  const int rounds = argc > 2 ? atoi(argv[2]) : 20;
  std::string doc = "{\"users\":[";
  for (int i = 0; i < entries; i++) {
    doc += (i ? "," : "") + std::string("{\"id\":") + std::to_string(i) +
           ",\"name\":\"user " + std::to_string(i) + "\",\"email\":\"user" +
           std::to_string(i) +
           "@example.com\",\"active\":" + ((i & 1) ? "true" : "false") +
           ",\"score\":" + std::to_string(i) +
           ".5,\"tags\":[\"a\",\"b\",\"c\"],\"profile\":{\"age\":" +
           std::to_string(i + 20) + ",\"theme\":\"dark\"}}";
  }
  doc += "]}\n";
  printf("doc: %zu bytes, %d entries, %d rounds\n", doc.size(), entries,
         rounds);
  for (int i = 0; i < 3; i++) {
    yeptris_json_tape t;
    if (yeptris_parse_json_tape(doc.data(), doc.size(), &t) != 0) {
      printf("parse failed\n");
      return 1;
    }
    yeptris_tape_free(&t);
  }
  double total_ms = 0;
  for (int i = 0; i < rounds; i++) {
    auto t0 = std::chrono::steady_clock::now();
    yeptris_json_tape t;
    if (yeptris_parse_json_tape(doc.data(), doc.size(), &t) != 0) {
      printf("parse failed round %d\n", i);
      return 1;
    }
    auto t1 = std::chrono::steady_clock::now();
    total_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    yeptris_tape_free(&t);
  }
  printf("mean: %.3f ms/pass (%.1f MB/s)\n", total_ms / rounds,
         (doc.size() / 1e6) / (total_ms / rounds / 1e3));
  for (int i = 0; i < 200; i++) {
    yeptris_json_tape t;
    yeptris_parse_json_tape(doc.data(), doc.size(), &t);
    yeptris_tape_free(&t);
  }
  printf("trace window done\n");
  return 0;
}
