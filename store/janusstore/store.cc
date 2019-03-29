// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/store.h"

namespace janusstore {

using namespace std;

Store::Store() : kv_store() { }

Store::~Store() { /* TODO delete kv_store? */ }

int Store::Get(uint64_t id, string key, string &value) {
    Debug("[%llu] GET %s", id, key.c_str());

    // TODO unsure if we need to deref key?
    unordered_map<string, string>::const_iterator ret = kv_store.find(key);

    if (ret == kv_store.end()) {
        // TODO this debug complains about type of key at compile time
    	Debug("Cannot find value for key %s", key.c_str());
    	return REPLY_FAIL;
    } else {
        Debug("Value: %s", ret->second.c_str());
        value = ret->second;
    	return REPLY_OK;
    }
}

int Put(uint64_t id, string key, string value) {
    Debug("[%llu] PUT <%s, %s>", id, key.c_str(), value.c_str());
    // compiler error here and i have no idea why
    kv_store.insert({{key, value}});
    return REPLY_OK;
}
} // namespace janusstore