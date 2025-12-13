#include "util.h"
#include <functional>
#include <latch>
#include <string>
#include <thread>
#include <atomic>
#include <type_traits>
#include <print>

inline thread_local auto t_thread_name = std::string{"unnamed_thread"};
inline thread_local auto t_thread_id = uint64_t{0};

class NamedJThread{
public:
    template <typename Callable, typename... Args>
    explicit NamedJThread(std::string name, Callable&& func, Args&&... args) {
        auto latch = std::latch{1};
        thread_ = std::jthread([name = std::move(name), func = std::forward<Callable>(func), &latch,this](std::stop_token st, auto&&... args) mutable{

            // 1. 设置操作系统级别的线程名（用于调试工具）
            // 2. 设置 Thread Local 变量
            t_thread_name = name;
            t_thread_id = UtilT::getThreadId();
            this->thread_id_ = std::jthread::id{pthread_t{t_thread_id}};
            this->thread_name_ = std::move(name);
            latch.count_down();
            // 3. 执行用户函数
            // 这里需要判断用户的函数是否接受 stop_token 作为第一个参数
            if constexpr (std::is_invocable_v<decltype(func), std::stop_token, decltype(args)...>) {
                // 如果用户函数接受 stop_token，我们就传给它
                std::invoke(std::move(func), st, std::forward<decltype(args)>(args)...);
            } else {
                // 如果用户函数不接受 stop_token，我们就不传
                std::invoke(std::move(func), std::forward<decltype(args)>(args)...);
            }

        }, std::forward<Args>(args)...);
        latch.wait();
    }

    template <typename Callable, typename... Args>
    requires (!std::convertible_to<Callable, std::string>)
    explicit NamedJThread(Callable&& func, Args&&... args)
        :NamedJThread("thread-" + std::to_string(s_thread_count_.fetch_add(1)), std::forward<Callable>(func), std::forward<Args>(args)...){
    }
// 默认构造
    NamedJThread() = default;

    // 禁止拷贝 (遵循 std::thread/jthread 语义)
    NamedJThread(const NamedJThread&) = delete;
    auto operator=(const NamedJThread&) -> NamedJThread& = delete;

    // 允许移动
    NamedJThread(NamedJThread&&) noexcept = default;
    auto operator=(NamedJThread&&) noexcept -> NamedJThread& = default;
    ~NamedJThread() = default;

    // 代理常用 std::jthread 方法
    void requestStop() noexcept { thread_.request_stop(); }
    [[nodiscard]] auto joinable() const noexcept -> bool { return thread_.joinable(); }
    void join() { thread_.join(); }
    void detach() { thread_.detach(); }
    [[nodiscard]] auto getId() const noexcept -> std::jthread::id { return thread_id_; }
    auto getStopSource() noexcept -> std::stop_source { return thread_.get_stop_source(); }
    auto getStopToken() noexcept -> std::stop_token { return thread_.get_stop_token(); }

    [[nodiscard]] auto printThreadInfo() {
        std::println("Thread Name: {}, Thread ID: {}", thread_name_, thread_id_);
    }

private:
    inline static std::atomic<int> s_thread_count_ = 0;
    std::jthread thread_;
    std::jthread::id thread_id_;
    std::string thread_name_;
};