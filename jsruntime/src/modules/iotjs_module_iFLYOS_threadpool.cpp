//
// Created by jan on 19-5-21.
//

#include "iotjs_module_iFLYOS_threadpool.h"

namespace iflyos{

ThreadPool::ThreadPool(size_t threads) : quit{false}{
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(
            [this] {
                for (;;) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this] { return quit || !this->tasks.empty(); });
                        if (quit && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
}

// the destructor joins all threads
void ThreadPool::safeQuit() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        quit = true;
    }
    condition.notify_all();
}

void ThreadPool::join(){
    for (std::thread &worker: workers)
        worker.join();
}

}