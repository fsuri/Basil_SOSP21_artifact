// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/benchmark/tpccClient.cc:
 *   Benchmarking client for tpcc.
 *
 **********************************************************************/

#include "lib/latency.h"
#include "lib/timeval.h"
#include "lib/tcptransport.h"
#include "store/common/truetime.h"
#include "store/common/frontend/async_client.h"
#include "store/common/frontend/async_adapter_client.h"
#include "store/janusstore/client.h"
#include "store/janusstore/transaction.h"
#include "store/benchmark/async/bench_client.h"
#include "store/benchmark/async/common/key_selector.h"
#include "store/benchmark/async/common/uniform_key_selector.h"
#include "store/benchmark/async/retwis/retwis_client.h"
#include "store/benchmark/async/tpcc/tpcc_client.h"

#include <gflags/gflags.h>

#include <vector>
#include <algorithm>

#include <pthread.h>

#define N 10

DEFINE_uint64(client_id, 0, "which client to run");

void SendTxn(janusstore::Client *client, janusstore::Transaction *txn_ptr, uint64_t ballot) {

	commit_callback ccb = [] (uint64_t committed) {
		printf("output commit from txn %d \r\n", committed);
		pthread_exit(NULL);
	};

	client->PreAccept(txn_ptr, ballot, ccb);
	printf("preaccept done\r\n");
}

int main(int argc, char **argv) {
	gflags::ParseCommandLineFlags(&argc, &argv, true);
	// transport is used to send messages btwn replicas and schedule msgs
	TCPTransport transport1(0.0, 0.0, 0, false);
	TCPTransport* transport_ptr1 = &transport1;

	TCPTransport transport2(0.0, 0.0, 0, false);
	TCPTransport* transport_ptr2 = &transport2;
	size_t sent = 0;

	// init client1 with closest replica 0
	janusstore::Client client1("./store/janus", 2, 0, transport_ptr1);
	janusstore::Client *client_ptr1 = &client1;

	// init client2 with closest replica 1
	janusstore::Client client2("./store/janus", 2, 1, transport_ptr1);
	janusstore::Client *client_ptr2 = &client2;

	// define some transactions
	janusstore::Transaction txn1(1234);
	janusstore::Transaction* txn_ptr1 = &txn1;
	txn_ptr1->addReadSet("key1");
	txn_ptr1->addReadSet("key2");
	txn_ptr1->addWriteSet("key3", "val3");
	txn_ptr1->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

	janusstore::Transaction txn2(4567);
	janusstore::Transaction* txn_ptr2 = &txn2;
	txn_ptr2->addReadSet("key3");
	txn_ptr2->addWriteSet("key1", "val1");
	txn_ptr2->addWriteSet("key2", "val2");
	txn_ptr2->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

	// printf("starting clients %d\n", FLAGS_client_id);

	transport1.Timer(
		0, [client_ptr1, txn_ptr1]() { SendTxn(client_ptr1, txn_ptr1, 0); });
	transport1.Timer(
		0, [client_ptr2, txn_ptr2]() { SendTxn(client_ptr2, txn_ptr2, 1); });
	transport1.Run();
	return 0;
}
