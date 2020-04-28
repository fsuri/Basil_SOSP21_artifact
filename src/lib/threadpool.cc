#include "lib/threadpool.h"

#include <thread>

ThreadPool::ThreadPool(uint32_t numThreads) {
  running = true;
  for (uint32_t i = 0; i < numThreads; i++) {
    std::thread t([this] {
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
        event_active(job.second->ev, 0, 0);
      }
    });
    t.detach();
  }
}

void ThreadPool::stop() {
  running = false;
  cv.notify_all();
}


void ThreadPool::EventCallback(evutil_socket_t fd, short what, void *arg) {
  // we want to run the callback in the main event loop
  EventInfo* info = (EventInfo*) arg;
  info->cb(info->r);
  event_free(info->ev);
  delete info;
}

void ThreadPool::dispatch(std::function<void*()> f, std::function<void(void*)> cb, event_base* libeventBase) {
  EventInfo* info = new EventInfo();
  info->cb = cb;
  info->ev = event_new(libeventBase, -1, 0, ThreadPool::EventCallback, info);
  event_add(info->ev, NULL);

  std::pair<std::function<void*()>, EventInfo*> job(f, info);

  std::lock_guard<std::mutex> lk(worklistMutex);
  worklist.push_back(job);
  cv.notify_one();
}
