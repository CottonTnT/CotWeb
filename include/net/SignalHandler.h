#include "net/EventLoop.h"
#include "net/Channel.h"
class SignalHandler{
public:
    using SignalCallback = std::function<void(int)>;
private:
    EventLoop* owner_loop_;
    int signal_fd_;
    sigset_t mask_;
    Uptr<Channel> channel_;
    SignalCallback signal_callback_;

public:
    explicit SignalHandler(EventLoop* loop);
    SignalHandler(const SignalHandler&)            = delete;
    SignalHandler(SignalHandler&&)                 = delete;
    auto operator=(const SignalHandler&) -> SignalHandler& = delete;
    auto operator=(SignalHandler&&) -> SignalHandler&      = delete;
    ~SignalHandler();

    void addSignal(int signo);
    void setCallback(SignalCallback cb);

    void start();

private:
    void signalChannelReadCB_();
};

