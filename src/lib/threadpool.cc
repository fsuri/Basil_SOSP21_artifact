#include "lib/threadpool.h"

#include <thread>
#include <sched.h>
#include <utility>


//TODO: make is so that all but the first core are used.
ThreadPool::ThreadPool() {
  // test_worklistMutex = new std::mutex;
  // test_cv = new std::condition_variable;

  int num_cpus = std::thread::hardware_concurrency();
  Debug("num cpus %d", num_cpus);
  uint32_t num_threads = (uint32_t) std::max(1, num_cpus - 1);
  running = true;
  for (uint32_t i = 0; i < num_threads; i++) {
    std::thread *t = new std::thread([this] {
      while (true) {
        std::pair<std::function<void*()>, EventInfo*> job;
        {
          // only acquire the lock in this block so that the
          // std::function execution is not holding the lock

          std::unique_lock<std::mutex> lock(this->worklistMutex);
          cv.wait(lock, [this] { return this->worklist.size() > 0 || !running; });
          if (!running) {
            break;
          }
          if (this->worklist.size() == 0) {
            continue;
          }
          job = this->worklist.front();
          this->worklist.pop_front();
        }

        job.second->r = job.first();
        // This _should_ be thread safe
        if(job.second){
            event_active(job.second->ev, 0, 0);
        }

      }
    });
    // Create a cpu_set_t object representing a set of CPUs. Clear it and mark
    // only CPU i as set.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    int rc = pthread_setaffinity_np(t->native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        Panic("Error calling pthread_setaffinity_np: %d", rc);
    }
    threads.push_back(t);
    t->detach();
  }
}

ThreadPool::~ThreadPool()
{
  stop();
  // delete test_worklistMutex;
  // delete test_cv;
}

void ThreadPool::stop() {
  running = false;
  cv.notify_all();
 // for(auto t: threads){
 //    t->join();
 //    delete t;
 // }
}


void ThreadPool::EventCallback(evutil_socket_t fd, short what, void *arg) {
  // we want to run the callback in the main event loop
  EventInfo* info = (EventInfo*) arg;
  info->cb(info->r);  //FS: If the main thread runs all of it, then the thread is not doing any function? I.e. the function needs to be run FIRST, before event_active is called, no? event_active invokes the callback which runs on the main loop.
  //could also only add new event with tv=0 once the function is complete. either is fine though.
  event_free(info->ev);
  //FreeEventInfo(info);
  delete info;
}

void ThreadPool::dispatch(std::function<void*()> f, std::function<void(void*)> cb, event_base* libeventBase) {
  EventInfo* info = new EventInfo(); //GetUnusedEventInfo();//
  info->cb = std::move(cb);
  info->ev = event_new(libeventBase, -1, 0, ThreadPool::EventCallback, info);
  //std::function<void(evutil_socket_t, short, void *)> evCB(std::bind(&ThreadPool::EventCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  //event_callback_fn fn = [this](evutil_socket_t fd, short what, void *arg){EventCallback(fd, what, arg);}
  //info->ev = event_new(libeventBase, -1, 0, fn, info);
  event_add(info->ev, NULL);

  //safe to moveinfo? dont expect it to do anything though, since its just a pointer
  std::pair<std::function<void*()>, EventInfo*> job(std::move(f), std::move(info));

  std::lock_guard<std::mutex> lk(worklistMutex);
  worklist.push_back(std::move(job));
  cv.notify_one();
}

void ThreadPool::detatch(std::function<void*()> f){
  EventInfo* info = nullptr;
  std::pair<std::function<void*()>, EventInfo*> job(std::move(f), info);
  std::lock_guard<std::mutex> lk(worklistMutex);
  worklist.push_back(job);
  cv.notify_one();
}

EventInfo* ThreadPool::GetUnusedEventInfo() {
  EventInfo *info;
  if (eventInfos.size() > 0) {
    info = eventInfos.back();
    eventInfos.pop_back();
  } else {
    info = new EventInfo();
  }
  return info;
}

void ThreadPool::FreeEventInfo(EventInfo *info) {
  eventInfos.push_back(info);
}
