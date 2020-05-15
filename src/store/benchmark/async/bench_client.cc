// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * benchmark.cpp:
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

#include "store/benchmark/async/bench_client.h"

#include "lib/latency.h"
#include "lib/message.h"
#include "lib/transport.h"
#include "lib/timeval.h"

#include <sys/time.h>
#include <string>
#include <sstream>
#include <algorithm>

DEFINE_LATENCY(op);

BenchmarkClient::BenchmarkClient(Transport &transport, uint64_t id,
		int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval, const std::string &latencyFilename) :
    id(id),
    tputInterval(tputInterval),
    transport(transport),
    rand(id),
    numRequests(numRequests), expDuration(expDuration),	delay(delay),
    warmupSec(warmupSec), cooldownSec(cooldownSec),
    latencyFilename(latencyFilename) {
	if (delay != 0) {
		Notice("Delay between requests: %ld ms", delay);
	} else {
    Notice("No delay between requests.");
  }
	started = false;
	done = false;
  cooldownStarted = false;
  if (numRequests > 0) {
	  latencies.reserve(numRequests);
  }
  _Latency_Init(&latency, "txn");
}

BenchmarkClient::~BenchmarkClient() {
}

void BenchmarkClient::Start(bench_done_callback bdcb) {
	n = 0;
  curr_bdcb = bdcb;
  transport.Timer(warmupSec * 1000, std::bind(&BenchmarkClient::WarmupDone,
			this));
  gettimeofday(&startTime, NULL);

  if (tputInterval > 0) {
		msSinceStart = 0;
		opLastInterval = n;
		transport.Timer(tputInterval, std::bind(&BenchmarkClient::TimeInterval,
				this));
  }
  Latency_Start(&latency);
  SendNext();
}

void BenchmarkClient::TimeInterval() {
  if (done) {
	  return;
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  msSinceStart += tputInterval;
  Notice("Completed %d requests at %lu ms", n-opLastInterval,
      (((tv.tv_sec*1000000+tv.tv_usec)/1000)/10)*10);
  opLastInterval = n;
  transport.Timer(tputInterval, std::bind(&BenchmarkClient::TimeInterval,
      this));
}

void BenchmarkClient::WarmupDone() {
  started = true;
  Notice("Completed warmup period of %d seconds with %d requests", warmupSec,
      n);
  n = 0;
}

void BenchmarkClient::CooldownDone() {
  done = true;

  char buf[1024];
  Notice("Finished cooldown period.");
  std::sort(latencies.begin(), latencies.end());

  uint64_t ns = latencies[latencies.size()/2];
  LatencyFmtNS(ns, buf);
  Notice("Median latency is %ld ns (%s)", ns, buf);

  ns = 0;
  for (auto latency : latencies) {
    ns += latency;
  }
  ns = ns / latencies.size();
  LatencyFmtNS(ns, buf);
  Notice("Average latency is %ld ns (%s)", ns, buf);

  ns = latencies[latencies.size()*90/100];
  LatencyFmtNS(ns, buf);
  Notice("90th percentile latency is %ld ns (%s)", ns, buf);

  ns = latencies[latencies.size()*95/100];
  LatencyFmtNS(ns, buf);
  Notice("95th percentile latency is %ld ns (%s)", ns, buf);

  ns = latencies[latencies.size()*99/100];
  LatencyFmtNS(ns, buf);
  Notice("99th percentile latency is %ld ns (%s)", ns, buf);

  curr_bdcb();
}

void BenchmarkClient::OnReply(int result) {
  IncrementSent(result);

  if (done) {
    return;
  }

  if (delay == 0) {
    Latency_Start(&latency);
    SendNext();
  } else {
    uint64_t rdelay = rand() % delay*2;
    transport.Timer(rdelay, std::bind(&BenchmarkClient::SendNext, this));
  }
}

void BenchmarkClient::StartLatency() {
  Latency_Start(&latency);
}

void BenchmarkClient::IncrementSent(int result) {
  if (started) {
    // record latency
    if (!cooldownStarted) {
      uint64_t ns = Latency_End(&latency);
      // TODO: use standard definitions across all clients for success/commit and failure/abort
      if (result == 0) { // only record result if success
        struct timespec curr;
        clock_gettime(CLOCK_MONOTONIC, &curr);
        if (latencies.size() == 0UL) {
          gettimeofday(&startMeasureTime, NULL);
          startMeasureTime.tv_sec -= ns / 1000000000ULL;
          startMeasureTime.tv_usec -= (ns % 1000000000ULL) / 1000ULL;
          //std::cout << "#start," << startMeasureTime.tv_sec << "," << startMeasureTime.tv_usec << std::endl;
        }
        uint64_t currNanos = curr.tv_sec * 1000000000ULL + curr.tv_nsec;
        std::cout << GetLastOp() << ',' << ns << ',' << currNanos << ',' << id << std::endl;
        latencies.push_back(ns);
      }
    }

    if (numRequests == -1) {
      struct timeval currTime;
      gettimeofday(&currTime, NULL);

      struct timeval diff = timeval_sub(currTime, startTime);
      if (diff.tv_sec > expDuration - cooldownSec && !cooldownStarted) {
        Debug("Starting cooldown after %ld seconds.", diff.tv_sec);
        Finish();
      } else if (diff.tv_sec > expDuration) {
        Debug("Finished cooldown after %ld seconds.", diff.tv_sec);
        CooldownDone();
      } else {
        Debug("Not done after %ld seconds.", diff.tv_sec);
      }
    } else if (n >= numRequests){ 
      CooldownDone();  
    }
  }

  n++;
}

void BenchmarkClient::Finish() {
  gettimeofday(&endTime, NULL);
  struct timeval diff = timeval_sub(endTime, startMeasureTime);

  std::cout << "#end," << diff.tv_sec << "," << diff.tv_usec << "," << id << std::endl;

  Notice("Completed %d requests in " FMT_TIMEVAL_DIFF " seconds", n,
      VA_TIMEVAL_DIFF(diff));

  if (latencyFilename.size() > 0) {
      Latency_FlushTo(latencyFilename.c_str());
  }

  if (numRequests == -1) {
    cooldownStarted = true;
  } else {
    CooldownDone();
  }
}
