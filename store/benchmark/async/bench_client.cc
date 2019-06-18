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

BenchmarkClient::BenchmarkClient(AsyncClient &client, Transport &transport,
		int numRequests, int expDuration, uint64_t delay, int warmupSec,
    int cooldownSec, int tputInterval, const std::string &latencyFilename) :
    tputInterval(tputInterval), client(client), transport(transport),
    numRequests(numRequests), expDuration(expDuration),	delay(delay),
    warmupSec(warmupSec), cooldownSec(cooldownSec),
    latencyFilename(latencyFilename) {
	if (delay != 0) {
		Notice("Delay between requests: %ld ms", delay);
	}
	started = false;
	done = false;
	cooldownDone = false;
	_Latency_Init(&latency, "op");
  if (numRequests > 0) {
	  latencies.reserve(numRequests);
  }
}

BenchmarkClient::~BenchmarkClient() {
}

void BenchmarkClient::Start() {
	n = 0;
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
  char buf[1024];
  cooldownDone = true;
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
}

void BenchmarkClient::OnReply(int result) {
  if (cooldownDone) {
    return;
  }

  if ((started) && (!done) && (n != 0)) {
    uint64_t ns = Latency_End(&latency);
    std::cout << GetLastOp() << ',' << ns << std::endl;
    latencies.push_back(ns);
    stats.Increment(GetLastOp() + "_" + std::to_string(result), 1);
    if (numRequests == -1) {
      struct timeval currTime;
      gettimeofday(&currTime, NULL);

      struct timeval diff = timeval_sub(currTime, startTime);
      if (diff.tv_sec > expDuration) {
        Finish();
      }
    } else { 
      if (n > numRequests) {
        Finish();
      }
    }
  }

  n++;
  if (delay == 0) {
    Latency_Start(&latency);
    SendNext();
  } else {
    uint64_t rdelay = rand() % delay*2;
    transport.Timer(rdelay, std::bind(&BenchmarkClient::SendNext, this));
  }
}

void BenchmarkClient::Finish() {
  gettimeofday(&endTime, NULL);

  struct timeval diff = timeval_sub(endTime, startTime);

  Notice("Completed %d requests in " FMT_TIMEVAL_DIFF " seconds", numRequests,
      VA_TIMEVAL_DIFF(diff));
  done = true;

  transport.Timer(cooldownSec * 1000, std::bind(&BenchmarkClient::CooldownDone,
      this));

  if (latencyFilename.size() > 0) {
      Latency_FlushTo(latencyFilename.c_str());
  }
}
