#include "store/benchmark/async/retwis/retwis_client.h"

#include "store/benchmark/async/retwis/add_user.h"
#include "store/benchmark/async/retwis/follow.h"
#include "store/benchmark/async/retwis/get_timeline.h"
#include "store/benchmark/async/retwis/post_tweet.h"

#include <iostream>

namespace retwis {

RetwisClient::RetwisClient(KeySelector *keySelector, Client &client,
    Transport &transport, int numRequests, int expDuration, uint64_t delay,
    int warmupSec, int cooldownSec, int tputInterval,
    const std::string &latencyFilename)
    : BenchmarkClient(client, transport, numRequests, expDuration, delay,
        warmupSec, cooldownSec, tputInterval, latencyFilename),
        keySelector(keySelector), currTxn(nullptr) {
}

RetwisClient::~RetwisClient() {
}

void RetwisClient::SendNext() {
  int ttype = std::rand() % 100;
  tid++;
  if (ttype < 5) {
    currTxn = new AddUser(tid, &client, keySelector);
    lastOp = "add_user";
  } else if (ttype < 20) {
    currTxn = new Follow(tid, &client, keySelector);
    lastOp = "follow";
  } else if (ttype < 50) {
    currTxn = new PostTweet(tid, &client, keySelector);
    lastOp = "post_tweet";
  } else {
    currTxn = new GetTimeline(tid, &client, keySelector);
    lastOp = "get_timeline";
  }
  currTxn->Execute([this](bool committed,
      std::map<std::string, std::string> readValues){
    delete this->currTxn;
    this->currTxn = nullptr;
    this->OnReply();
  });
}

std::string RetwisClient::GetLastOp() const {
  return lastOp;
}

} //namespace retwis
