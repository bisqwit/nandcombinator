#ifndef bqtAsyncPoolHH
#define bqtAsyncPoolHH

#include <future>
#include <memory>
#include <atomic>
#include <mutex>
#include <list>

struct async_task_pool
{
    std::mutex               lock{};
    std::condition_variable  state_changed{};
    std::condition_variable  runner_registered{};
    std::atomic<std::size_t> num_running{0};
    std::atomic<std::size_t> num_runners{0};
    std::atomic<bool>        ending{false};
    std::atomic<bool>        waiting{false};

    std::vector<std::thread>                threads;
    std::vector<std::packaged_task<void()>> tasks;
    std::size_t              N=1;

    async_task_pool() : N{std::thread::hardware_concurrency()}
    {
        if(0)fprintf(stderr, "Pool %p is creating threads\n", (const void*)this);
        threads.reserve(N);
        for(unsigned a=0; a<N; ++a) CreateRunner();
        { std::unique_lock<std::mutex> lk(lock);
          while(num_runners < threads.size()) runner_registered.wait(lk); }
    }

    ~async_task_pool()
    {
        ending = true;
        if(0)fprintf(stderr, "Pool %p ending (%u tasks, running %u, ending %d)\n",
            (const void*)this,
            unsigned(tasks.size()),
            unsigned(num_running),
            unsigned(ending));
        // Signal all the workers that they might be able to terminate now
        { std::unique_lock<std::mutex> lk(lock);
          while(num_runners < threads.size()) runner_registered.wait(lk); }
        { std::lock_guard<std::mutex>{lock}; state_changed.notify_all(); }
        // Wait until they do
        for(auto& t: threads) t.join();
    }

    void wait()
    {
        waiting = true;
        std::unique_lock<std::mutex> lk(lock);
        for(;;)
        {
            unsigned tasks_before = tasks.size();
            if(!tasks_before) break;
            if(0)fprintf(stderr, "Pool %p waits, before = %u\n", (const void*)this, tasks_before);
            state_changed.wait(lk);
            unsigned tasks_after  = tasks.size();
            if(0)fprintf(stderr, "Pool %p: after = %u\n", (const void*)this, tasks_after);
            if(tasks_after)
            {
                lk.unlock();
                state_changed.notify_one();
                lk.lock();
            }
        }
        waiting = false;
    }

    template<typename Func, typename... Args>
    auto run(Func func, Args&&... args)
    {
        typedef std::result_of_t<Func(Args...)> restype;
        std::unique_lock<std::mutex> lk(lock);
        /*if(tasks.size() >= threads.size() && threads.size() < N)
        {
            CreateRunner();
            while(num_runners < threads.size()) runner_registered.wait(lk);
        }*/
        std::packaged_task<restype()> task([=]() mutable { return func(args...); });
        auto result = task.get_future();
        if(0)fprintf(stderr, "Pool %p: adding task\n", (const void*)this);
        tasks.emplace_back([task=std::move(task)]() mutable {task();});
        lk.unlock();
        // Signal one of the workers that there may be new task
        state_changed.notify_one();
        //lk.lock(); lk.unlock();
        return std::move(result);
    }

    template<typename Container, typename Func, typename... Args>
    void run_save(Container& target, Func func, Args&&... args)
    {
        target.emplace_back(run(func, std::forward<Args>(args)...));
    }

    template<typename Func, typename... Args>
    void parallelize_for(std::size_t limit, Func func, Args&&... args)
    {
        std::vector<std::future<void>> tasks;
        tasks.reserve(N);
        for(std::size_t ind=0; ind < limit; )
        {
            std::size_t remain = limit-ind;
            unsigned cap = N-1;
            if(remain/cap <= 2) cap = N;
            for(unsigned b=0; b< cap; ++b)
            {
                std::size_t begin = ind + (b  )*remain/N;
                std::size_t end   = ind + (b+1)*remain/N;
                if(end > begin)
                    run_save(tasks, func, begin, end, std::forward<Args>(args)...);
            }
            ind += cap*remain/N;
        }
        wait_saved(std::move(tasks));
    }

    template<typename Container>
    static void wait_saved(Container&& target)
    {
        for(auto& t: target) t.get();
        target.clear();
    }

    async_task_pool(const async_task_pool&) = delete;
    async_task_pool(async_task_pool&&) = delete;
    void operator=(const async_task_pool&) = delete;
    void operator=(async_task_pool&&) = delete;

private:
    void CreateRunner()
    {
        threads.emplace_back( [this] { Runner(); });
    }
    void Runner()
    {
        if(0)fprintf(stderr, "Pool %p Runner %lX launched\n",
            (const void*)this, long(std::hash<std::thread::id>()(std::this_thread::get_id())));
        ++num_runners;
        for(;;)
        {
            // When !ending:
            //      tasks.empty(): wait
            //      running>=N:    wait
            //      running<N:     RUN
            //
            // When ending:
            //      tasks.empty(): END
            //      running>=N:    wait
            //      running<N:     RUN

            std::unique_lock<std::mutex> lk(lock);
            for(;;)
            {
                if(tasks.empty() && ending) return;
                if(!tasks.empty() && num_running < N) break;
                if(0)fprintf(stderr, "Pool %p Runner %lX waiting (%u tasks, running %u, ending %d)\n",
                    (const void*)this,
                    long(std::hash<std::thread::id>()(std::this_thread::get_id())),
                    unsigned(tasks.size()),
                    unsigned(num_running),
                    unsigned(ending));
                runner_registered.notify_all();
                state_changed.wait(lk);
            }
            // Update statistics
            ++num_running;
            if(0)fprintf(stderr, "Pool %p Runner %lX launching (%u tasks, running %u, ending %d)\n",
                (const void*)this,
                long(std::hash<std::thread::id>()(std::this_thread::get_id())),
                unsigned(tasks.size()),
                unsigned(num_running),
                unsigned(ending));
            // Remove the task from the pending list
            auto task = std::move( tasks.front() );
            tasks.erase(tasks.begin());
            lk.unlock();
            // Run task
            task();
            // Update statistics
            lk.lock();
            --num_running;
            if(0)fprintf(stderr, "Pool %p Runner %lX completed (%u tasks, running %u, ending %d)\n",
                (const void*)this,
                long(std::hash<std::thread::id>()(std::this_thread::get_id())),
                unsigned(tasks.size()),
                unsigned(num_running),
                unsigned(ending));
            lk.unlock();
            // Signal one of the workers that we have fewer remaining tasks
            if(!waiting) state_changed.notify_one();
            else         state_changed.notify_all();
        }
        --num_runners;
        if(0)fprintf(stderr, "Pool %p Runner %lX ends\n",
            (const void*)this, long(std::hash<std::thread::id>()(std::this_thread::get_id())));
    }
};

#endif
