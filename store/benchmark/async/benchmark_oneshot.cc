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
void SendTxn(janusstore::Client *client, size_t *sent) {
	commit_callback ccb = [client, sent] (uint64_t committed) {
		if (*sent < N) {
			// essentially repeatedly sends a transaction through preaccept
			*sent += 1;
			SendTxn(client, sent);
		}
		printf("output commit from txn %d \r\n", committed);
	};

	janusstore::Transaction txn;
	janusstore::Transaction* txn_ptr = &txn;
	txn_ptr->addReadSet("key1");
	txn_ptr->addReadSet("key2");
	txn_ptr->addWriteSet("key3", "val3");
	client->PreAccept(txn_ptr, 0, ccb);
	printf("preaccept done\r\n");
}

int main(int argc, char **argv) {
	// transport is used to send messages btwn replicas and schedule msgs
	UDPTransport transport(0.0, 0.0, 0, false);
	UDPTransport* transport_ptr = &transport;
	size_t sent = 0;
	janusstore::Client client("shard", 2, 1, transport_ptr);
	janusstore::Client *client_ptr = &client;
	transport.Timer(0, [client_ptr, &sent]() { SendTxn(client_ptr, &sent); });
	transport.Run();
	return 0;
}
