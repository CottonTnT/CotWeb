-- 项目名称与版本
set_project("MyWeb")
set_version("0.1.0")

-- 设置需要的最低 xmake 版本
set_xmakever("2.8.0")


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

-- 控制 examples、tests、tools 的构建
option("buildExamples")
    set_default(true)
    set_showmenu(true)
    set_description("Build examples")
option_end()

option("buildExamples")
    set_default(true)
    set_showmenu(true)
    set_description("Build tests")
option_end()

option("buildTools")
    set_default(true)
    set_showmenu(true)
    set_description("Build tools")
option_end()


-- 导入其他配置与依赖脚本 (需将原 .cmake 转换为对应 .lua 逻辑)
includes("cmake/pansOptions.lua")
includes("cmake/pansDependencies.lua")
includes("cmake/functions.lua")

-- 添加子目录模块
includes("pans")

if has_config("buildExamples") then
    includes("examples")
end

if has_config("buildTests") then
    includes("tests")
end

if has_config("buildTools") then
    includes("tools")
end


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