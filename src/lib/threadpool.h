#ifndef _LIB_THREADPOOL_H_
#define _LIB_THREADPOOL_H_

#include <list>
#include <mutex>
#include <functional>
#include <condition_variable>

#include <event2/event.h>

class ThreadPool {

public:

  ThreadPool(uint32_t numThreads);

  void stop();

  void dispatch(std::function<void*()> f, std::function<void(void*)> cb, event_base* libeventBase);

private:
  struct EventInfo {
      event* ev;
      std::function<void(void*)> cb;
      void* r;
  };

  static void EventCallback(evutil_socket_t fd, short what, void *arg);

  std::mutex worklistMutex;
  std::condition_variable cv;
  std::list<std::pair<std::function<void*()>, EventInfo*> > worklist;
  bool running;

};

#endif  // _LIB_THREADPOOL_H_
