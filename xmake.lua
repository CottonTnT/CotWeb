-- 定义项目
set_project("sylar-web")
set_version("0.1.0")

-- 设置语言标准
set_languages("c++23")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "build"})
-- add_rules("mode.debug", "mode.release")
set_policy("build.warning", true)
set_warnings("all")
-- 添加包含路径

if is_mode("debug") then 
    set_symbols("debug")
    set_optimize("none")
end

if is_mode("release") then 
    set_symbols("none")
    set_optimize("fastest")
end

target("common-lib")
    set_kind("static")
    set_targetdir("lib")
    add_includedirs("include")
    add_files("srcs/common/*.cpp")

target("logger")
    set_kind("static")
    set_targetdir("lib")
    add_deps("common-lib")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_files("srcs/logger/*.cpp")
    remove_files("logappenderdefine.h", "logdefine.h")

target("fiber")
    set_targetdir("lib")
    add_deps("common-lib")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_files("srcs/fiber/*.cpp")

target("muduo-net")
    set_kind("static")
    set_targetdir("lib")
    -- add_deps("common-lib", "logger", "fiber")
    add_deps("common-lib")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_files("srcs/net/*.cpp")
    -- remove_files("timerqueue.cpp")  -- 先移除有问题的文件

target("discard")
    set_kind("binary")
    add_deps("muduo-net", "common-lib")
    add_includedirs("include", "/usr/local/include", "examples/simple/discard")
    add_files("examples/simple/discard/*.cpp")
    add_syslinks("pthread")


target("daytime")
    set_kind("binary")
    add_deps("muduo-net", "common-lib")
    add_includedirs("include", "/usr/local/include", "examples/simple/daytime")
    add_files("examples/simple/daytime/*.cpp")
    add_syslinks("pthread")

target("printer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib")
    add_includedirs("include", "/usr/local/include", "examples/simple/printer")
    add_files("examples/simple/printer/*.cpp")
    add_syslinks("pthread")


target("TimerServer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple/time/server")
    add_files("examples/simple/time/server/*.cpp")
    add_syslinks("pthread")

target("testlogger")
    set_kind("binary")
    add_deps("logger", "common-lib")
    add_files("test/testlogger.cpp")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_syslinks("pthread")

target("testfiber")
    set_kind("binary")
    add_deps("logger", "common-lib")
    add_files("test/testfiber.cpp")
    add_files("srcs/fiber/*.cpp")
    remove_files("srcs/fiber/scheduler.cpp")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_syslinks("pthread")
    -- add_defines("NO_DEBUG") //开启调试日志


target("testscheduler")
    set_kind("binary")
    add_deps("logger", "common-lib")
    -- add_cxxflags("-fsanitize=address,undefined,leak", {force = true}) //use to detect memory leaks
    -- add_ldflags("-fsanitize=address,undefined,leak", {force = true})

    add_files("test/testscheduler.cpp")
    add_files("srcs/fiber/*.cpp")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_syslinks("pthread")
    -- add_defines("NO_DEBUG") //开启调试日志

target("testyaml")
    set_kind("binary")
    add_files("test/testyaml.cpp")
    add_includedirs("/usr/local/include")
    add_syslinks("yaml-cpp")

target("testenv")
    set_kind("binary")
    add_files("test/testenv.cpp")
    -- add_files("srcs/util.cpp")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    -- add_syslinks("pthread")

target("testNamedJThread")
    set_kind("binary")
    add_files("test/t_namedjtread.cpp")
    add_files("srcs/common/util.cpp")
    -- add_files("srcs/common/NamedJThread.h")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_syslinks("pthread")

target("fun")
    set_kind("binary")
    add_files("test/wow.cpp")
    add_includedirs("include", "/usr/local/include")
    -- add_cxxflags("-O2", {force = true})
    add_syslinks("pthread")
    -- add_cxxflags("-O0", {force = true})
    -- add_defines("DEBUG", {public = true}) 设置编译宏 -DXXX