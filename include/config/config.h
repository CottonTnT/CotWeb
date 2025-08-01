#pragma once

#include "common.h"
#include "configvar.hpp"
#include "configvarbase.h"
#include "mutex.h"
#include <memory>
#include <stdexcept>

namespace ConfigT{

class Config {
public:
    using ConfigVarMap = std::unordered_map<std::string, Sptr<ConfigVarBase>>;
    using RWMutexType =  Cot::RWMutex;
    inline static constexpr auto* c_ValidChars = "abcdefghikjlmnopqrstuvwxyz._0123456789";
    /**
     * @brief 获取/创建对应参数名的配置参数
     * @param[in] name 配置参数名称
     * @param[in] default_value 参数默认值
     * @param[in] description 参数描述
     * @details 获取参数名为name的配置参数,如果存在直接返回
     *          如果不存在, 创建参数配置并用default_value赋值
     * @return 返回对应的配置参数,如果参数名存在但是类型不匹配则返回nullptr
     * @exception 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
     */
    template <typename T>
    static auto LookupOr(const std::string& name,
                       const T& default_value,
                       const std::string& description)
        ->  Sptr<ConfigVar<T>>
    {
        auto _ = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>{ GetMutex_() };
        
        if (auto it = GetDatas_().contains(name))
        {
            if (auto ptr = std::dynamic_pointer_cast<ConfigVar<T>>(GetDatas_()[name]); ptr)
            {
                // todo: logger here;
                return ptr;
            }
            else
            {
                // todo: log here
                return nullptr;
            }
        }
        // then create a default value of this config
        //check the validation of name
        if (name.find_first_not_of(c_ValidChars) != std::string::npos)
        {
            //todo: log here
            throw std::invalid_argument{name};
        }
        // then create
        auto new_var = std::make_shared<ConfigVar<T>>(name, default_value, description);
        GetDatas_()[name] = new_var;
        return new_var;
    }

    template <typename T>
    static auto Lookup(const std::string& name)
        -> Sptr<ConfigVar<T>>
    {
        auto _ = Cot::LockGuard<RWMutexType, Cot::ReadLockTag>{GetMutex_()};
        if (not GetDatas_().contains(name))
        {
            return nullptr;
        }
        return std::dynamic_pointer_cast<ConfigVar<T>>(GetDatas_()[name]);
    }

    /**
     * @brief init the config module using YAML::Node
     */
    static auto LoadFromYaml(const YAML::Node& root)
        -> void;

    /**
     * @brief load the config file from the dir path point to
     * @param force: Whether to force the loading of configuration files. If not forced, the file will be skipped if the recorded modification time has not changed.
     */
    static auto LoadFromConfDir(const std::string& path, bool force = false)
        -> void;


    static auto LookupBase(const std::string& name)
        -> Sptr<ConfigVarBase>;

    static auto Visit(std::function<void(Sptr<ConfigVarBase>)> callback)
        -> void;

private:

    static auto GetDatas_()
        -> ConfigVarMap&
    {
        static auto  s_Datas = ConfigVarMap{};
        return s_Datas;
    }
    static auto GetMutex_()
        -> RWMutexType&
    {
        static auto s_Mtx = RWMutexType{};
        return s_Mtx;
    }

};

} //namespace ConfigT