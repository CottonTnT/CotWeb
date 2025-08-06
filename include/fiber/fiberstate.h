#pragma once

// enum class FiberState {
//     INIT, //初始化状态
//     HOLD, //暂停状态
//     EXEC, //执行中状态
//     TERM, //结束状态
//     READY, //可执行状态
//     EXCEPT, // 异常状态
// };


    /**
     * @brief 协程状态
     * @details 在sylar基础上进行了状态简化，只定义三态转换关系，也就是协程要么正在运行(RUNNING)，
     * 要么准备运行(READY)，要么运行结束(TERM)。不区分协程的初始状态，初始即READY。不区分协程是异常结束还是正常结束，
     * 只要结束就是TERM状态。也不区别HOLD状态，协程只要未结束也非运行态，那就是READY状态。
     * 一个程序的异常状态应有程序自身处理！！！
     * todo: 也许需要HOLD状态，模拟操作系统中线程中的sleep状态
     */


enum class FiberState {
    /// 就绪态，yield之后的状态
    READY,
    /// 运行态，resume之后的状态
    RUNNING,
    /// 结束态，协程的回调函数执行完之后为TERM状态
    TERM
};