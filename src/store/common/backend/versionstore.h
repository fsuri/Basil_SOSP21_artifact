
/////////////////////NOTE: IT IS UNSAFE IF MORE THAN 1 WRITER!!!!
//XXX IF TRYING TO ADD MORE WRITERS: ADD MUTEXES BACK

#ifndef _VERSIONED_KV_STORE_H_
#define _VERSIONED_KV_STORE_H_

#include "lib/assert.h"
#include "lib/message.h"
#include "store/common/timestamp.h"

#include <set>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <sys/time.h>
#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_hash_map.h"
#include "tbb/concurrent_set.h"

template<class T, class V>
class VersionedKVStore {
 public:
  VersionedKVStore();
  ~VersionedKVStore();

  long int lock_time;
  int KVStore_size();
  void KVStore_Reserve(int size);
  int ReadStore_size();

  bool get(const std::string &key, std::pair<T, V> &value);
  bool get(const std::string &key, const T &t, std::pair<T, V> &value);
  bool getRange(const std::string &key, const T &t, std::pair<T, T> &range);
  bool getLastRead(const std::string &key, T &readTime);
  bool getLastRead(const std::string &key, const T &t, T &readTime);
  bool getCommittedAfter(const std::string &key, const T &t,
      std::vector<std::pair<T, V>> &values);
  void put(const std::string &key, const V &v, const T &t);
  void commitGet(const std::string &key, const T &readTime, const T &commit);
  bool getUpperBound(const std::string& key, const T& t, T& result);

 private:
  struct VersionedValue {
    T write;
    V value;

    VersionedValue(const T &commit) : write(commit) { };
    VersionedValue(const T &commit, const V &val) : write(commit), value(val) { };

    friend bool operator> (const VersionedValue &v1, const VersionedValue &v2) {
        return v1.write > v2.write;
    };
    friend bool operator< (const VersionedValue &v1, const VersionedValue &v2) {
        return v1.write < v2.write;
    };
  };

  /* Global store which keeps key -> (timestamp, value) list. */

  //XXX make sets/map concurrent too.
  tbb::concurrent_unordered_map<std::string, std::set<VersionedValue>> store;

  tbb::concurrent_unordered_map<std::string, std::map<T, T>> lastReads;

  bool inStore(const std::string &key);
  void getValue(const std::string &key, const T &t,
      typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator &it);
};

template<class T, class V>
VersionedKVStore<T, V>::VersionedKVStore() { lock_time = 0;}

template<class T, class V>
VersionedKVStore<T, V>::~VersionedKVStore() { }

template<class T, class V>
int VersionedKVStore<T, V>::KVStore_size() {
    return store.size();
 }

 template<class T, class V>
 void VersionedKVStore<T, V>::KVStore_Reserve(int size) {
     //store.reserve(size);
     return;
  }


 template<class T, class V>
 int VersionedKVStore<T, V>::ReadStore_size() {
     return lastReads.size();
  }

template<class T, class V>
bool VersionedKVStore<T, V>::inStore(const std::string &key) {
  //std::shared_lock lock(storeMutex);
  return store.find(key) != store.end() && store[key].size() > 0;
}

template<class T, class V>
void VersionedKVStore<T, V>::getValue(const std::string &key, const T &t,
    typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator &it) {
  //std::shared_lock lock(storeMutex);
  VersionedKVStore<T, V>::VersionedValue v(t);
  it = store[key].upper_bound(v);

  // if there is no valid version at this timestamp
  if (it == store[key].begin()) {
      it = store[key].end();
  } else {
      it--;
  }
}

/* Returns the most recent value and timestamp for given key.
 * Error if key does not exist. */
template<class T, class V>
bool VersionedKVStore<T, V>::get(const std::string &key,
    std::pair<T, V> &value) {
  // check for existence of key in store
  if (inStore(key)) {

    VersionedKVStore<T, V>::VersionedValue v = *(store[key].rbegin());
    value = std::make_pair(v.write, v.value);
    return true;
  }
  return false;
}

/* Returns the value valid at given timestamp.
 * Error if key did not exist at the timestamp. */
template<class T, class V>
bool VersionedKVStore<T, V>::get(const std::string &key, const T &t,
    std::pair<T, V> &value) {

  if (inStore(key)) {
    typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator it;
    getValue(key, t, it);

    if (it != store[key].end()) {
      value = std::make_pair((*it).write, (*it).value);
      return true;
    }
  }
  return false;
}

template<class T, class V>
bool VersionedKVStore<T, V>::getRange(const std::string &key, const T &t,
    std::pair<T, T> &range) {

  if (inStore(key)) {
    typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator it;
    getValue(key, t, it);

    if (it != store[key].end()) {
      range.first = (*it).write;
      it++;
      if (it != store[key].end()) {
        range.second = (*it).write;
      }
      return true;
    }
  }
  return false;
}

template<class T, class V>
bool VersionedKVStore<T, V>::getUpperBound(const std::string& key, const T& t, T& result) {

  VersionedKVStore<T, V>::VersionedValue v(t);
  auto it = store[key].upper_bound(v);

  // if there is no valid version at this timestamp
  if (it == store[key].end()) {
    return false;
  } else {
    result = (*it).write;
    return true;
  }

}

template<class T, class V>
void VersionedKVStore<T, V>::put(const std::string &key, const V &value,
    const T &t) {
  // Key does not exist. Create a list and an entry.

  store[key].insert(VersionedKVStore<T, V>::VersionedValue(t, value));
}

/*
 * Commit a read by updating the timestamp of the latest read txn for
 * the version of the key that the txn read.
 */
template<class T, class V>
void VersionedKVStore<T, V>::commitGet(const std::string &key,
    const T &readTime, const T &commit) {
  // Hmm ... could read a key we don't have if we are behind ... do we commit this or wait for the log update?
  if (inStore(key)) {
    typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator it;
    getValue(key, readTime, it);


    if (it != store[key].end()) {
      // figure out if anyone has read this version before
      if (lastReads.find(key) != lastReads.end() &&
        lastReads[key].find((*it).write) != lastReads[key].end() &&
        lastReads[key][(*it).write] < commit) {
        lastReads[key][(*it).write] = commit;
      }
    }
  } // otherwise, ignore the read
}

template<class T, class V>
bool VersionedKVStore<T, V>::getLastRead(const std::string &key, T &lastRead) {

  if (inStore(key)) {
    VersionedValue v = *(store[key].rbegin());

    if (lastReads.find(key) != lastReads.end() &&
      lastReads[key].find(v.write) != lastReads[key].end()) {
      lastRead = lastReads[key][v.write];
      return true;
    }
  }
  return false;
}

/*
 * Get the latest read for the write valid at timestamp t
 */
template<class T, class V>
bool VersionedKVStore<T, V>::getLastRead(const std::string &key, const T &t,
    T &lastRead) {

  if (inStore(key)) {
    typename std::set<VersionedKVStore<T, V>::VersionedValue>::iterator it;
    getValue(key, t, it);

    // TODO: this ASSERT seems incorrect. Why should we expect to find a value
    //    at given time t? There is no constraint on t, so we have no guarantee
    //    that a valid version exists.
    // UW_ASSERT(it != store[key].end());

    // figure out if anyone has read this version before

    if (lastReads.find(key) != lastReads.end() &&
      lastReads[key].find((*it).write) != lastReads[key].end()) {
      lastRead = lastReads[key][(*it).write];
      return true;
    }
  }
  return false;
}

template<class T, class V>
bool VersionedKVStore<T, V>::getCommittedAfter(const std::string &key,
    const T &t, std::vector<std::pair<T, V>> &values) {

  VersionedKVStore<T, V>::VersionedValue v(t);
  const auto itr = store.find(key);
  if (itr != store.end()) {
    auto setItr = itr->second.upper_bound(v);
    while (setItr != itr->second.end()) {
      values.push_back(std::make_pair(setItr->write, setItr->value));
      setItr++;
    }
    return true;
  }
  return false;
}


#endif  /* _VERSIONED_KV_STORE_H_ */
