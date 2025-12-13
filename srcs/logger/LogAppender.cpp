#include "logger/LogAppender.h"
#include "common/alias.h"
#include <chrono>
#include <filesystem>
#include <ios>
#include <sys/select.h>
#include <system_error>
#include <time.h>

/* ======================== StdoutAppender ======================== */
void StdoutAppender::log(const LogFormatter& fmter, const LogEvent& event)
{
    fmter.format(std::cout, event);
}

/* ======================== FileAppender ======================== */
RollingFileAppender::RollingFileAppender(std::string filename,
                                         size_t max_bytes_,
                                         Seconds roll_interval)
    : filename_ {std::move(filename)}
    , basename_ {std::filesystem::path {filename_}.filename().string()}
    , max_bytes_ {max_bytes_}
    , roll_interval_ {roll_interval}
{
    openFile_();
}

RollingFileAppender::~RollingFileAppender()
{
    auto _ = std::lock_guard<std::mutex>(mutex_);
    if (filestream_.is_open())
    {
        filestream_.close();
    }
}

auto RollingFileAppender::openFile_()
    ->void

{
    // 清除错误标识，准备重新打开
    reopen_error_ = false;
    // 设置流：在 failbit 或 badbit 被设置时抛出异常
    filestream_.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    try
    {
        // 使用 std::ios::app 模式打开，确保写入时追加到文件末尾, std::ios::binary 确保 '\n' 写入1字节
        filestream_.open(filename_, std::ios::out | std::ios::app | std::ios::binary);
    } catch (const std::ios_base::failure& e)
    {
        const auto& ec = e.code();
        std::cerr << "--- 文件操作失败 ---" << std::endl;
        std::cerr << "错误描述 (what()): " << e.what() << std::endl;
        std::cerr << "错误码 (error_code): " << ec.value() << std::endl;
        std::cerr << "错误类别 (category): " << ec.category().name() << std::endl;
        throw std::system_error{ec};
    }
    last_open_time_ = Clock::now();
    // 获取当前文件大小
    offset_ = filestream_.tellp();
}

auto RollingFileAppender::rollFile_()
    -> void
{
    if (not filestream_.is_open())
    {
        // 如果文件未打开，直接尝试打开新文件
        openFile_();
    }

    // 1. 关闭当前文件
    filestream_.close();

    // 2. 生成带时间戳的新文件名
    std::string new_filename = getNewLogFileName_();

    // 3. 重命名文件 (旧文件名 -> 新文件名)
    // 使用 std::rename 进行原子操作
    if (std::rename(filename_.c_str(), new_filename.c_str()) != 0)
    {
        throw std::system_error{std::error_code{errno, std::system_category()},
                                "重命名日志文件失败: " + filename_ + " -> " + new_filename};
    }

    // 4. 打开新的日志文件
    return openFile_();
}
// **判断是否需要滚动**
auto RollingFileAppender::shouldRoll_() const
    -> bool
{
    // 1. 大小检查
    if (offset_ > max_bytes_)
    {
        return true;
    }
    // 2. 时间检查 (如果上次打开时间超过滚动间隔)
    auto now     = Clock::now();
    auto elapsed = std::chrono::duration_cast<Seconds>(now - last_open_time_);
    return elapsed > roll_interval_;
}
auto RollingFileAppender::getNewLogFileName_() const
    -> std::string
{
    auto p = std::filesystem::path(filename_);

    // 1. 获取文件名主体 (不含扩展名) 和扩展名
    auto stem = p.stem().string(); // 例如：对于 "app.log"，得到 "app"

    auto extension = p.extension().string(); // 例如：对于 "app.log"，得到 ".log"

    // 2. 获取时间戳字符串
    auto now        = std::chrono::time_point_cast<Seconds>(std::chrono::system_clock::now());
    auto zoned_time = std::chrono::zoned_time<Seconds> {
        std::chrono::current_zone(),
        now};
    auto time_point_str = std::format("{:%Y-%m-%d-%H-%M-%S}", zoned_time.get_local_time());
    // 3. 构建新的文件名: stem.YYYYMMDD-HHMMSS.extension
    // 注意：如果原文件名没有扩展名，extension会是空字符串
    std::string new_filename = stem;
    new_filename += "." + std::string(time_point_str);
    new_filename += extension;

    // 4. 组合路径：使用 parent_path() 确保新文件仍在原目录
    // 原始文件名包含路径，所以我们需要父路径
    return (p.parent_path() / new_filename).string();
}

void RollingFileAppender::log(const LogFormatter& fmter, const LogEvent& event)
{
    // 1. 线程安全保护
    auto _ = std::lock_guard<std::mutex> {mutex_};

    // 2. 检查是否需要滚动 (时间或大小)
    if (shouldRoll_())
    {
        rollFile_();
    }
    // 4. 格式化并写入
    // muduo 的 FileAppender 通常使用非格式化的 append(const char* data, int len)
    // 但这里我们沿用您最初的 LogFormatter 接口
    auto totol_size = fmter.format(filestream_, event);

    // 5. 更新写入字节数
    // 这是一个近似值，更精确的字节数可以通过 filestream_.tellp() 获得，
    offset_ += totol_size; // 假设每次写入平均 1KB，此处仅为演示目的

    // 6.更新 Flush计数器
    flush_count_++;

    // 7. 检查 Flush 条件

    auto now = Clock::now();
    
    // 检查是否超过时间间隔
    auto time_to_flush = (now - last_flush_time_) > c_flush_interval_seconds;
    
    // 检查是否达到最大写入次数
    bool count_to_flush = flush_count_ >= c_flush_max_appends;

    if (time_to_flush || count_to_flush)
    {
        // 调用 std::ostream::flush() 将数据从 C++ 缓冲区推送到操作系统
        filestream_.flush(); 

        // 重置状态
        last_flush_time_ = now;
        flush_count_ = 0;
    }
}
