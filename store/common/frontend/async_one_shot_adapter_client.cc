#include "store/common/frontend/async_one_shot_adapter_client.h"

AsyncOneShotAdapterClient::AsyncOneShotAdapterClient(OneShotClient *client) :
    client(client) {
}

AsyncOneShotAdapterClient::~AsyncOneShotAdapterClient() {
}

void AsyncOneShotAdapterClient::Execute(AsyncTransaction *txn,
    execute_callback ecb) {
  OneShotTransaction oneShotTxn;
  Operation op;
  std::map<std::string, std::string> emptyReadValues;
  for (size_t opCount = 0; ; ++opCount) {
    op = txn->GetNextOperation(opCount, emptyReadValues);
    if (op.type == COMMIT || op.type == ABORT) {
      break;
    } else if (op.type == GET) {
      oneShotTxn.AddRead(op.key);
    } else if (op.type == PUT) {
      Debug("wheeeee %s, %s", op.key.c_str(), op.value.c_str());
      oneShotTxn.AddWrite(op.key, op.value);
    }
  }
  if (op.type == COMMIT) {
    // TODO oneShotTxn is destructed after calling Execute; need to make sure
    // all clients avoid assuming that oneShotTxn will live long
    client->Execute(&oneShotTxn, ecb);
  }
}

