/***********************************************************************
 *
 * Copyright 2021 Florian Suri-Payer <fs435@cornell.edu>
 *                Matthew Burke <matthelb@cs.cornell.edu>
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
#include "lib/latency.h"
#include "lib/message.h"
#include "store/indicusstore/indicus-proto.pb.h"
#include "store/common/common-proto.pb.h"
#include "lib/crypto.h"

#include <gflags/gflags.h>

#include <random>

DEFINE_uint64(size, 1000, "size of data to verify.");
DEFINE_uint64(iterations, 100, "number of iterations to measure.");


//DEFINE_string(signature_alg, "ecdsa", "algorithm to benchmark (options: ecdsa, ed25519, rsa, secp256k1, donna)");

void GenerateRandomString(uint64_t size, std::random_device &rd, std::string &s) {
  s.clear();
  for (uint64_t i = 0; i < size; ++i) {
    s.push_back(static_cast<char>(rd()));
  }
}

void GenerateRandomString(uint64_t size, std::random_device &rd, std::string* s) {
  s->clear();
  for (uint64_t i = 0; i < size; ++i) {
    s->push_back(static_cast<char>(rd()));
  }
}

int main(int argc, char *argv[]) {
  gflags::SetUsageMessage("benchmark signature verification.");
	gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::random_device rd;

  struct Latency_t serP1;
  struct Latency_t deserP1;
  struct Latency_t serP1Reply;
  struct Latency_t deserP1Reply;
  struct Latency_t serWB;
  struct Latency_t deserWB;

  struct Latency_t batchLat;
  _Latency_Init(&serP1, "serP1");
  _Latency_Init(&deserP1, "deserP1");
  _Latency_Init(&serP1Reply, "serP1Reply");
  _Latency_Init(&deserP1Reply, "deserP1Reply");
  _Latency_Init(&serWB, "serWB");
  _Latency_Init(&deserWB, "deserWB");




  Notice("===================================");
  Notice("Running indicusstore::proto bench for %lu iterations with data size %lu.",
      FLAGS_iterations, FLAGS_size);
  for (uint64_t i = 0; i < FLAGS_iterations; ++i) {
      std::string s;
      GenerateRandomString(FLAGS_size, rd, s);

      indicusstore::proto::Phase1* p1 = new indicusstore::proto::Phase1();
      p1->set_req_id(0);

      indicusstore::proto::Transaction *txn = new indicusstore::proto::Transaction();
      txn->set_client_id(0);
      txn->set_client_seq_num(0);
      txn->add_involved_groups(0);

      ReadMessage *read = txn->add_read_set();
      *read->mutable_key() = s;
      TimestampMessage *tm = new TimestampMessage();
      tm->set_id(0);
      tm->set_timestamp(1);
      *read->mutable_readtime() = *tm;

      //ts.serialize((read->mutable_readtime());
      WriteMessage *write = txn->add_write_set();
      write->set_key(s);
      write->set_value(s);

      *txn->mutable_timestamp() = *tm;
      *p1->mutable_txn() = *txn;

  //////////////

      indicusstore::proto::Phase1Reply* p1reply = new indicusstore::proto::Phase1Reply();
      p1reply->set_req_id(0);
      indicusstore::proto::ConcurrencyControl *cc = new indicusstore::proto::ConcurrencyControl();
      cc->set_ccr(indicusstore::proto::ConcurrencyControl::COMMIT);
      *cc->mutable_txn_digest() = s;
      cc->set_involved_group(1);

      crypto::PrivKey* privKey = crypto::GenerateKeypair(crypto::KeyType::DONNA, false).first;
      p1reply->mutable_signed_cc()->set_process_id(0);
      cc->SerializeToString(p1reply->mutable_signed_cc()->mutable_data());
      *p1reply->mutable_signed_cc()->mutable_signature() = crypto::Sign(privKey, s);



  ///////////
      indicusstore::proto::Writeback* wb = new indicusstore::proto::Writeback();

      indicusstore::proto::GroupedSignatures *grp_sigs = new indicusstore::proto::GroupedSignatures();
      indicusstore::proto::Signatures *sigs = new indicusstore::proto::Signatures();
      for(int i =0; i <6; ++i){
        indicusstore::proto::Signature *sig = sigs->add_sigs();
        sig->set_process_id(0);
        *sig->mutable_signature() = crypto::Sign(privKey, s);
      }
      (*grp_sigs->mutable_grouped_sigs())[0] = *sigs;

      wb->set_decision(indicusstore::proto::CommitDecision::COMMIT);
      *wb->mutable_txn_digest() = s;
      *wb->mutable_p1_sigs() = *grp_sigs ;
      wb->set_p2_view(0);


    ///////////////////
     std::string p1_str;
     std::string p1_reply_str;
     std::string wb_str;


      Latency_Start(&serP1);
        p1->SerializeToString(&p1_str);
      Latency_End(&serP1);

      Latency_Start(&deserP1);
        p1->ParseFromString(p1_str);
      Latency_End(&deserP1);

      Latency_Start(&serP1Reply);
        p1reply->SerializeToString(&p1_reply_str);
      Latency_End(&serP1Reply);

      Latency_Start(&deserP1Reply);
        p1reply->ParseFromString(p1_reply_str);
      Latency_End(&deserP1Reply);

      Latency_Start(&serWB);
        wb->SerializeToString(&wb_str);
      Latency_End(&serWB);

      Latency_Start(&deserWB);
        wb->ParseFromString(wb_str);
      Latency_End(&deserWB);

      delete wb;
      delete p1;
      delete p1reply;
      delete txn;
      //delete write;
      //delete read;
      delete tm;
  }


  Latency_Dump(&serP1);
  Latency_Dump(&deserP1);
  Latency_Dump(&serP1Reply);
  Latency_Dump(&deserP1Reply);
  Latency_Dump(&serWB);
  Latency_Dump(&deserWB);

  Notice("===================================");
  //Latency_Dump(&signBLat);
  //Latency_Dump(&verifyBLat);
  //Latency_Dump(&hashLat);
  //Latency_Dump(&blake3Lat);
  return 0;
}
