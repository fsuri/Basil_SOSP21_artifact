#ifndef _LIB_THREADPOOL_H_
#define _LIB_THREADPOOL_H_

#include "assert.h"
#include <list>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <thread>
#include <event2/event.h>

class ThreadPool {

public:

  ThreadPool();
  virtual ~ThreadPool();
  // copy constructor panics
  ThreadPool(const ThreadPool& tp) { Panic("Unimplemented"); }

  void stop();

  void dispatch(std::function<void*()> f, std::function<void(void*)> cb, event_base* libeventBase);
  void detatch(std::function<void*()> f);

private:
  struct EventInfo {
      event* ev;
      std::function<void(void*)> cb;
      void* r;
  };

  static void EventCallback(evutil_socket_t fd, short what, void *arg);

  // std::mutex* test_worklistMutex;
  // std::condition_variable* test_cv;

  std::mutex worklistMutex;
  std::condition_variable cv;
  std::list<std::pair<std::function<void*()>, EventInfo*> > worklist;
  bool running;
  std::vector<std::thread*> threads;

};

#endif  // _LIB_THREADPOOL_H_
