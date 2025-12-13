#pragma once

#include "EventFixedBuffer.hpp"
#include "logger/Logger.h"
#include <algorithm>
#include <vector>
#include <condition_variable>
#include <print>
#include <latch>

class AsyncLogger : public Logger {
public:
    using EventBuffer    = EventFixedBuffer<>;
    using EventBufferPtr = std::unique_ptr<EventBuffer>;

public:
    // 构造函数：初始化缓冲区和日志文件，启动日志线程
    explicit AsyncLogger(int flush_interval = 3) // 3秒刷新一次
        : flush_interval_ {flush_interval}
        , running_(false)
        , current_buffer_(std::make_unique<EventFixedBuffer<>>()) // 初始化双缓冲
        , next_buffer_(std::make_unique<EventFixedBuffer<>>())
    {
        // 初始化备用缓冲列表，用于收集应用线程写满的缓冲
        buffers_to_write_.reserve(16);
    }

    AsyncLogger(const AsyncLogger&)            = delete;
    AsyncLogger(AsyncLogger&&)                 = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;
    AsyncLogger& operator=(AsyncLogger&&)      = delete;
    ~AsyncLogger()
    {
        if (running_)
        {
            stop();
        }
    }

    // 核心接口：供应用线程调用，只接收 LogEvent
    void append(LogEvent event)
    {
        auto _ = std::lock_guard<std::mutex> {mutex_};

        // 尝试写入当前缓冲区（检查事件数量）
        if (current_buffer_->available() > 0)
        {
            current_buffer_->append(std::move(event));
        }
        else
        {
            // todo: 限制 buffers_to_write_的 大小
            //  缓冲区已满
            buffers_to_write_.push_back(std::move(current_buffer_));
            // 交换缓冲区逻辑
            if (next_buffer_)
            {
                current_buffer_ = std::move(next_buffer_);
            }
            else
            {
                current_buffer_ = std::make_unique<EventBuffer>();
            }
            current_buffer_->append(std::move(event)); // 写入新的 current_buffer_
            // 直接通知后端日志线程干活
            cond_.notify_one();
        }
    }

    // 启动日志线程
    void start()
    {
        running_ = true;
        thread_  = std::thread(&AsyncLogger::threadFunc_, this);
        latch_.wait();
    }

    // 停止日志线程
    void stop()
    {
        running_ = false;
        cond_.notify_one(); // 唤醒线程使其退出循环
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

private:
    // 后台日志线程执行的函数 (消费者)
    void threadFunc_()
    {
        latch_.count_down();
        // 预分配用于交换的缓冲区，避免在日志线程中频繁分配内存
        auto new_buffer1        = std::make_unique<EventBuffer>();
        auto new_buffer2        = std::make_unique<EventBuffer>();
        auto buffers_to_process = std::vector<EventBufferPtr> {}; // 用于处理待写入的缓冲区
        buffers_to_process.reserve(16);

        // 主循环，直到 stop() 被调用
        while (running_)
        {
            {
                // 等待条件变量唤醒或超时
                auto lock = std::unique_lock<std::mutex>(mutex_);

                // 等待：直到超时 (flush_interval_) 或被通知有数据
                if (buffers_to_write_.empty())
                {
                    cond_.wait_for(lock, std::chrono::seconds(flush_interval_));
                }

                // 1. 将 current_buffer_ 移入待处理列表 (即使未满，也到时间刷新了)
                buffers_to_write_.push_back(std::move(current_buffer_));

                // 2. 将新分配的 new_buffer1 替换为新的 current_buffer_
                current_buffer_ = std::move(new_buffer1);

                // 3. 交换待写入列表：将应用线程的数据移交给日志线程
                buffers_to_process.swap(buffers_to_write_);
                // 此时，应用线程可以继续向 buffers_to_write_ 写入，互不影响

                // 4. 将空的 new_buffer2 替换为新的 next_buffer_
                if (next_buffer_ == nullptr)
                {
                    next_buffer_ = std::move(new_buffer2);
                }
            } // 互斥锁释放

            // --- 日志线程开始 I/O 操作 (无锁) ---
            if (buffers_to_process.size() > 25)
            {
                // 如果待处理缓冲区过多，说明生产速度远超消费速度，可能出现日志堆积
                // 这里简单丢弃部分日志以防内存占用过高

                auto now        = std::chrono::time_point_cast<Seconds>(std::chrono::system_clock::now());
                auto zoned_time = std::chrono::zoned_time<Seconds> {
                    std::chrono::current_zone(),
                    now};
                auto time_point_str = std::format("{:%Y-%m-%d-%H-%M-%S}", zoned_time.get_local_time());
                std::println("Dropper log messages at {}, {} larger buffer", time_point_str, buffers_to_process.size() - 2);
                buffers_to_process.erase(
                    buffers_to_process.begin() + 2,
                    buffers_to_process.end());
            }

            // 5. 将所有缓冲区内容写入文件
            for (const auto& buf : buffers_to_process)
            {
                auto data = buf->getEventSpan();
                std::ranges::for_each(data, [this](const LogEvent& event) {
                    this->log(event);
                });
            }

            // 6. 清理并回收缓冲区
            if (buffers_to_process.size() > 2)
            {
                // 如果待处理缓冲区过多，回收多余内存，但至少保留两个空的供交换
                buffers_to_process.resize(2);
            }

            // 7. 将两个已处理的空缓冲区放回预分配指针，供下次交换使用
            if (new_buffer1 != nullptr)
            {
                new_buffer1 = std::move(buffers_to_process.back());
                buffers_to_process.pop_back();
            }
            if (new_buffer2 != nullptr)
            {
                new_buffer2 = std::move(buffers_to_process.back());
                buffers_to_process.pop_back();
            }

            // 清空待处理列表
            buffers_to_process.clear();
            // 由 appender 内部的 flush 会确保数据写入
        }
    }

private:
    // 配置
    const int flush_interval_; // 强制刷新间隔（秒）

    // 线程和同步
    std::thread thread_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::latch latch_{1};

    // 双缓冲机制的核心
    EventBufferPtr current_buffer_;                // 当前应用线程正在写入的缓冲区
    EventBufferPtr next_buffer_;                   // 备用缓冲区（用于减少应用线程等待时间）
    std::vector<EventBufferPtr> buffers_to_write_; // 已写满，等待后台线程写入的缓冲区列表
};