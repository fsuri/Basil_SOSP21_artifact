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
  void dispatch_local(std::function<void*()> f, std::function<void(void*)> cb);
  void detatch(std::function<void*()> f);
  void issueCallback(std::function<void(void*)> cb, event_base* libeventBase);

private:

  struct EventInfo {
      EventInfo(ThreadPool* tp): tp(tp) {}
      event* ev;
      std::function<void(void*)> cb;
      void* r;
      ThreadPool* tp;
  };

  static void EventCallback(evutil_socket_t fd, short what, void *arg);
  static void* combiner(std::function<void*()> f, std::function<void(void*)> cb);

  EventInfo* GetUnusedEventInfo();
  void FreeEventInfo(EventInfo *info);
  event* GetUnusedEvent(event_base* libeventBase, EventInfo* info);
  void FreeEvent(event* event);


  std::mutex worklistMutex;
  std::condition_variable cv;
  std::vector<EventInfo*> eventInfos;
  std::vector<event*> events;
  std::list<std::pair<std::function<void*()>, EventInfo*> > worklist;
  bool running;
  std::vector<std::thread*> threads;

};

#endif  // _LIB_THREADPOOL_H_
