// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * benchmark.h:
 *   simple replication benchmark client
 *
 * Copyright 2013 Dan R. K. Ports  <drkp@cs.washington.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/
#ifndef BENCHMARK_CLIENT_H
#define BENCHMARK_CLIENT_H

#include "store/common/frontend/async_client.h"
#include "store/common/stats.h"
#include "lib/latency.h"
#include "lib/transport.h"
#include <random>

typedef std::function<void()> bench_done_callback;

class BenchmarkClient {
 public:
  BenchmarkClient(Transport &transport, uint32_t seed, int numRequests,
      int expDuration, uint64_t delay, int warmupSec, int cooldownSec,
      int tputInterval, const std::string &latencyFilename = "");
  virtual ~BenchmarkClient();

  void Start(bench_done_callback bdcb);
  void OnReply(int result);

  void StartLatency();
  virtual void SendNext() = 0;
  void IncrementSent(int result);
  inline bool IsFullyDone() { return done; }

  struct Latency_t latency;
  bool started;
  bool done;
  bool cooldownStarted;
  int tputInterval;
  std::vector<uint64_t> latencies;

  inline const Stats &GetStats() const { return stats; }
 protected:
  virtual std::string GetLastOp() const = 0;

  inline std::mt19937 &GetRand() { return rand; }
  
  Stats stats;
  Transport &transport;
 private:
  void Finish();
  void WarmupDone();
  void CooldownDone();
  void TimeInterval();

  std::mt19937 rand;
  int numRequests;
  int expDuration;
  uint64_t delay;
  int n;
  int warmupSec;
  int cooldownSec;
  struct timeval startTime;
  struct timeval endTime;
  struct timeval startMeasureTime;
  string latencyFilename;
  int msSinceStart;
  int opLastInterval;
  bench_done_callback curr_bdcb;
};

#endif /* BENCHMARK_CLIENT_H */
