-- 定义项目
set_project("MyWeb")
set_version("0.1.0")

-- 设置需要的最低 xmake 版本
set_xmakever("2.8.0")

-- 项目名称与版本
set_project("pans")
set_version("0.1.0")

-- 设置支持的编译模式，默认 Debug
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.minsizerel")
set_defaultmode("debug")

-- 设置语言标准 (gnu++20 对应 Enable C++ Extensions 的 C++20)
set_languags("c11", "gnu++20")

-- 如果主机是 Linux，指定使用 GCC/G++ 编译器
if is_host("linux") then
    set_config("cc", "/usr/bin/gcc")
    set_config("cxx", "/usr/bin/g++")
end

-- 自定义构建选项
option("build_examples")
    set_default(true)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("build_tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build tests")
option_end()

option("build_tools")
    set_default(true)
    set_showmenu(true)
    set_description("Build tools")
option_end()

-- 定义编译选项规则（相当于 CMake 中的 INTERFACE 库 target）
rule("compiler_options")
    on_config(function (target)
        -- GCC / Clang 通用编译选项
        if target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            target:add("cxflags", "-Wall", "-Wextra", "-Wpedantic", "-fno-strict-aliasing", "-Wno-builtin-macro-redefined")
        end

        -- Linux 平台下的编译/链接选项
        if is_plat("linux") and target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            target:add("cxflags", "-fPIC")
            target:add("ldflags", "-rdynamic")
        end
    end)

    -- 根据构建模式添加定义与编译参数
    on_load(function (target)
        if is_mode("debug") then
            target:add("defines", "PANS_DEBUG")
            target:add("cxflags", "-O0", "-g3", "-ggdb")
            
            -- 代码覆盖率选项（默认关闭，可使用 xmake f --coverage=y 开启）
            if has_config("coverage") then
                target:add("cxflags", "--coverage")
                target:add("ldflags", "--coverage")
            end
        elseif is_mode("release", "releasedbg") then
            target:add("defines", "NDEBUG")
            target:add("cxflags", "-O2", "-fno-omit-frame-pointer")
            
            if is_mode("releasedbg") then
                target:add("cxflags", "-g")
            end
        end
    end)
rule_end

-- 导入其他配置与依赖脚本 (需将原 .cmake 转换为对应 .lua 逻辑)
includes("cmake/pansOptions.lua")
includes("cmake/pansDependencies.lua")
includes("cmake/functions.lua")

-- 添加子目录模块
includes("pans")

if has_config("build_tests") then
    includes("tests")
end

if has_config("build_tools") then
    includes("tools")
end

-- 导出 CMake 配置文件 (供 CMake 项目调用)
-- Xmake 内置规则会自动生成 pansConfig.cmake / pansTargets.cmake 并安装到指定位置
add_rules("utils.install.cmake_importfiles")

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


target("TimeServer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple/time/server")
    add_files("examples/simple/time/server/*.cpp")
    add_syslinks("pthread")


target("TimeClient")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple/time/client")
    add_files("examples/simple/time/client/*.cpp")
    add_syslinks("pthread")

target("EchoServer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple/echo")
    add_files("examples/simple/echo/*.cpp")
    add_syslinks("pthread")


target("ChargenServer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple/chargen")
    add_files("examples/simple/chargen/*.cpp")
    add_syslinks("pthread")

target("ChatServer")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include")
    add_files("examples/chat/server/*.cpp")
    add_syslinks("pthread")

target("ChatClient")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include")
    add_files("examples/chat/client/*.cpp")
    add_syslinks("pthread")

target("SignalHandler")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs(
        "include", 
        "/usr/local/include", 
        "examples/simple/signalhandler"
    )
    add_files("examples/simple/signalhandler/*.cpp")
    add_syslinks("pthread")

target("AllInOne")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/simple")
    add_files(
        "examples/simple/**/*.cpp", 
        "examples/simple/allinone/allinone.cc"
    )
    remove_files(
        "examples/simple/**/main.cpp"
    )
    add_syslinks("pthread")


target("DownloadFile")
    set_kind("binary")
    add_deps("muduo-net", "common-lib", "logger")
    add_includedirs("include", "/usr/local/include", "examples/filetransfer")
    add_files(
        "examples/filetransfer/*.cpp" 
    )
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