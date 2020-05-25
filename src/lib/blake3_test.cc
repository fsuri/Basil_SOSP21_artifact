#include "lib/batched_sigs.h"
#include "lib/assert.h"
#include "lib/latency.h"
#include "lib/blake3.h"

#include <gflags/gflags.h>

#include <string>
#include <vector>
#include <iostream>
#include <random>

DEFINE_uint64(message_size, 32, "size of data to verify.");
DEFINE_uint64(trials, 1000, "number of iterations to measure.");
DEFINE_uint64(batch_size, 128, "number of iterations to measure.");
void GenerateRandomString(uint64_t size, std::mt19937 &rd, std::string *s) {
  s->clear();
  for (uint64_t i = 0; i < size; ++i) {
    s->push_back(static_cast<char>(rd()));
  }
}

int main(int argc, char *argv[]) {
  gflags::SetUsageMessage("merkle hash benchmark.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  Latency_t iterLat;
  _Latency_Init(&iterLat, "iter");

  Latency_t chunkLat;
  _Latency_Init(&chunkLat, "chunk");


  uint8_t *data = new uint8_t[FLAGS_batch_size * FLAGS_message_size];
  std::mt19937 rd;
  std::vector<std::string *> messages;
  uint8_t out[BLAKE3_OUT_LEN];
  blake3_hasher h;
  for (size_t k = 0; k < FLAGS_trials; ++k) {

    for (size_t j = 0; j < FLAGS_batch_size; ++j) {
      if (k == 0) {
        messages.push_back(new std::string());
      }
      GenerateRandomString(FLAGS_message_size, rd, messages[j]);
      memcpy(data + (j * FLAGS_message_size), &(*messages[j])[0],
          FLAGS_message_size);
    }

    blake3_hasher_init(&h);
    Latency_Start(&iterLat);
    for (size_t i = 0; i < messages.size(); ++i) {
      blake3_hasher_update(&h, &(*messages[i])[0],
          messages[i]->length());
    }
    blake3_hasher_finalize(&h, out, BLAKE3_OUT_LEN);
    Latency_End(&iterLat);
   
    
    blake3_hasher_init(&h);
    Latency_Start(&chunkLat);
    blake3_hasher_update(&h, data, FLAGS_batch_size * FLAGS_message_size);
    blake3_hasher_finalize(&h, out, BLAKE3_OUT_LEN);
    Latency_End(&chunkLat);
  }

  double iterAvg = static_cast<double>(iterLat.dists['=']->total) / FLAGS_batch_size / FLAGS_trials;
  double chunkAvg = static_cast<double>(chunkLat.dists['=']->total) / chunkLat.dists['=']->count;
  
  std::cerr << "iter update avg " << iterAvg  << " ns" << std::endl;
  std::cerr << "chunk update avg " << chunkAvg  << " ns" << std::endl;

  double breakEven = chunkAvg / iterAvg;
  std::cerr << "# iters completed during single chunk " << breakEven << std::endl;

  double ratio = FLAGS_batch_size / breakEven;
  std::cerr << "speedup " << ratio << std::endl;

  Latency_Dump(&iterLat);
  Latency_Dump(&chunkLat);

  double min = 1e9;
  size_t minBatch = 0;
  size_t minBranch = 0;
  for (size_t n = 1; n <= FLAGS_batch_size; n <<= 1) {
  for (size_t i = 2; i <= n; i <<= 1) {
    size_t height = 0;
    size_t x = n;
    while (x > 0) {
      x = x / i;
      ++height;
    }
    Latency_t treeLat;
    _Latency_Init(&treeLat, ("tree_" + std::to_string(i)).c_str());
    for (size_t k = 0; k < FLAGS_trials; ++k) {
      for (size_t j = 0; j < FLAGS_batch_size; ++j) {
        GenerateRandomString(FLAGS_message_size, rd, messages[j]);
        memcpy(data + (j * FLAGS_message_size), &(*messages[j])[0],
            FLAGS_message_size);
      }

      Latency_Start(&treeLat);
      for (size_t j = 0; j < height - 1; ++j) {
        blake3_hasher_init(&h);
        blake3_hasher_update(&h, data + (i * BLAKE3_OUT_LEN),
            i * BLAKE3_OUT_LEN);
        blake3_hasher_finalize(&h, out, BLAKE3_OUT_LEN);
      }
      Latency_End(&treeLat);
    }
    // Latency_Dump(&treeLat);
    double treeAvg = static_cast<double>(treeLat.dists['=']->total) / FLAGS_trials;
    std::cerr << "batch size " << n << " avg tree " << i << " lat " << treeAvg << " ns" << std::endl;
    if (treeAvg < min) {
      min = treeAvg;
      minBatch = n;
      minBranch = i;
    }
  }
  }

  std::cerr << "min batch size " << minBatch << " branch factor " << minBranch << " avg lat " << min << std::endl;

  delete [] data;
  for (auto msg : messages) {
    delete msg;
  }

  return 0;
}
