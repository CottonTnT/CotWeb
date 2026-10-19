
-- 定义编译选项规则（相当于 CMake 中的 INTERFACE 库 target）
rule("compiler_options")
    on_config(function (target)
        -- GCC / Clang 通用编译选项
        if target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            target:add(
                "cxflags", 
                "-Wall", 
                "-Wextra", 
                "-Wpedantic", 
                "-fno-strict-aliasing", 
                "-Wno-builtin-macro-redefined"
            )
        end

        -- Linux 平台下的编译/链接选项
        if is_plat("linux") and target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            target:add(
                "cxflags", 
                "-fPIC"
            )
            target:add(
                "ldflags", 
                "-rdynamic"
            )
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
rule_end()