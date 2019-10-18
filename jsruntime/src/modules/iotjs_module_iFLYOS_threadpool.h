#ifndef MESSAGER_IOTJS_MODULE_IFLYOS_THREADPOOL_H
#define MESSAGER_IOTJS_MODULE_IFLYOS_THREADPOOL_H

#include <functional>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <future>


namespace iflyos{
class ThreadPool {
    public:
    ThreadPool(size_t);

    template < class
    F, class... Args >
    auto enqueue(F &&f, Args &&... args) -> std::future < typename
    std::result_of<F(Args...)>::type > ;

    void safeQuit();
    void join();

    private:
    // need to keep track of threads so we can join them
    std::vector <std::thread> workers;
    // the task queue
    std::queue <std::function<void()>> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool quit;
};

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename
    std::result_of<F(Args...)>::type;

    auto task = std::make_shared < std::packaged_task < return_type() > >
                (std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future <return_type> res = task->get_future();
    {
        std::unique_lock <std::mutex> lock(queue_mutex);

// don't allow enqueueing after stopping the pool
        if (quit)

            throw
        std::runtime_error(

            "enqueue on stopped ThreadPool");

        tasks.emplace([task]()
        {
            (*task)();
        });
    }
    condition.

        notify_one();

    return res;
}

class ScopeGuard {
public:
    template<class Callable>
    ScopeGuard(Callable &&fn) : fn_(std::forward<Callable>(fn)) {}

    ScopeGuard(ScopeGuard &&other) : fn_(std::move(other.fn_)) {
        other.fn_ = nullptr;
    }

    ~ScopeGuard() {
        // must not throw
        if (fn_) fn_();
    }

    ScopeGuard(const ScopeGuard &) = delete;
    void operator=(const ScopeGuard &) = delete;

private:
    std::function<void()> fn_;
};

}
#endif//MESSAGER_IOTJS_MODULE_IFLYOS_THREADPOOL_H