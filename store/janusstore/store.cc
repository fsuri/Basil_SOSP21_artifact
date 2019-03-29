// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
#include "store/janusstore/store.h"

namespace janusstore {

using namespace std;

Store::Store() { /* TODO need to initialize store? */ }

Store::~Store() { delete store; }

string Store::Get(uint64_t id, const string &key, string &value) {
    Debug("[%lu] GET %s", id, key.c_str());

    // TODO unsure if we need to deref key?
    unordered_map<string, string>::const_iterator value = store.find(key);

    if (value == store.end()) {
    	Debug("Cannot find value for key %s", key);
    	// TODO may need to modify the return spec
    	return to_string(REPLY_FAIL);
    } else {
        Debug("Value: %s", value.second.c_str());
    	return value.second;
    }
}

int Put(uint64_t id, const string &key, const string &value) {
    Debug("[%lu] PUT <%s, %s>", id, key.c_str(), value.c_str());

    // no idea if this is right
    pair<string, string> entry (key, value);
    store.insert(entry);

    return REPLY_OK;
}
} // namespace janusstore