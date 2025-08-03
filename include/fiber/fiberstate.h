#pragma once

enum class FiberState {
    INIT, //初始化状态
    HOLD, //暂停状态
    EXEC, //执行中状态
    TERM, //结束状态
    READY, //可执行状态
    EXCEPT, // 异常状态
};