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

#define N 10

DEFINE_uint64(client_id, 0, "which client to run");

void SendTxn(janusstore::Client *client, size_t *sent) {
	commit_callback ccb = [] (uint64_t committed) {
		printf("output commit from txn %d \r\n", committed);
	};

	janusstore::Transaction txn(1234, "asdf", 1);
	janusstore::Transaction* txn_ptr = &txn;
	txn_ptr->addReadSet("key1");
	txn_ptr->addReadSet("key2");
	txn_ptr->addWriteSet("key3", "val3");
	txn_ptr->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

	client->PreAccept(txn_ptr, 0, ccb);
	printf("preaccept done\r\n");
}

void SendTxn2(janusstore::Client *client, size_t *sent) {
	commit_callback ccb = [] (uint64_t committed) {
		printf("output commit from txn %d \r\n", committed);
	};

	janusstore::Transaction txn(4567, "asdf", 1);
	janusstore::Transaction* txn_ptr = &txn;
	txn_ptr->addReadSet("key3");
	txn_ptr->addWriteSet("key1", "val1");
	txn_ptr->addWriteSet("key2", "val2");
	txn_ptr->setTransactionStatus(janusstore::proto::TransactionMessage::PREACCEPT);

	client->PreAccept(txn_ptr, 0, ccb);
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
	janusstore::Client client1("./store/janus", 1, 0, transport_ptr1);
	janusstore::Client *client_ptr1 = &client1;

	// init client2 with closest replica 1
	janusstore::Client client2("./store/janus", 1, 1, transport_ptr2);
	janusstore::Client *client_ptr2 = &client2;

	transport1.Timer(500, [client_ptr1, &sent]() { SendTxn(client_ptr1, &sent); });
	transport2.Timer(500, [client_ptr2, &sent]() { SendTxn2(client_ptr2, &sent); });

	printf("starting client %d\n", FLAGS_client_id);
	if (FLAGS_client_id == 0) {
	    transport1.Run();
	} else {
		transport2.Run();
	}
	return 0;
}
