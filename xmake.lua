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


-- 添加测试目标
target("testlogger")
    set_kind("binary")
    add_files("test/testlogger.cpp")
    add_files("srcs/logger/*.cpp")
    add_files("srcs/util.cpp")
    add_includedirs("include")
    add_includedirs("/usr/local/include")
    add_syslinks("pthread")

-- target("testlogformatter")
--     set_kind("binary")
--     add_deps("logger") -- 依赖sylarweb库
--     add_files("srcs/logger/testlogger.cpp")
--     add_includedirs("include")
--     add_includedirs("/usr/local/include")


target("fun")
    set_kind("binary")
    add_files("test/wow.cpp")
    add_includedirs("include", "/usr/local/include")
    -- add_cxxflags("-O2", {force = true})
    add_syslinks("pthread")
    -- add_cxxflags("-O0", {force = true})
    -- add_defines("DEBUG", {public = true}) 设置编译宏 -DXXX