#pragma once

#include "common/mutex.h"
#include "config/configvarbase.h"
#include "configutil.h"

namespace ConfigT{
template <typename T, typename FromStr = UtilT::LexicalCast<std::string, T>, typename ToStr = UtilT::LexicalCast<T, std::string>>
class ConfigVar:ConfigVarBase {
public:
    using RWMutexType = Cot::RWMutex;
    using OnChangeCallback = std::function<void(const T& old_value, const T& new_value)>;
    using CallBackFunId = std::uint64_t;

    /**
     * @brief 通过参数名,参数值,描述构造ConfigVar
     * @param[in] name 参数名称有效字符为[0-9a-z_.]
     */
    ConfigVar(std::string name, T&& default_value, std::string description_ = "")
        : ConfigVarBase(std::move(name), std::move(description_))
        , val_(std::forward<T>(default_value))
    {
    }

    /**
     * @brief convert the val_ to YAML String
     * @exception throw exception when failed on convertion
     */
    auto ToString()
        -> std::string override
    {
        try
        {
            auto lock = Cot::LockGuard<RWMutexType, Cot::ReadLockTag>(mtx_);
            return ToStr{}(val_);
        } catch (std::exception& e)
        {
            //todo log here
        }
    }

    /**
     * @brief convert the YAML String to val_
     * @exception throw exception when failed on convertion
     */
    auto FromString(const std::string& val)
        -> bool override
    {
        try
        {
            auto lock = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>(mtx_);
            SetValue(FromStr{}(val));
            return true;
        } catch (std::exception& e)
        {
            //todo log here
        }
        return false;
    }

    auto SetValue(T val)
        -> void
    {
        auto lk_guard = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>{mtx_};
        val_ = val;
    }
    
    auto GetValue()
        ->  T
    {
        auto lk_guard = Cot::LockGuard<RWMutexType, Cot::ReadLockTag>{mtx_};
        return val_;
    }

    auto GetTypeName() const
        -> std::string override
    {
        return GetTypeName();
    }

    auto AddListener(OnChangeCallback callback)
        -> CallBackFunId
    {
        static auto s_Id = CallBackFunId{0};
        auto _ = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>(mtx_);
        callbacks_[s_Id] = callback;
        s_Id ++;
        return s_Id;
    }

    auto DelListener(CallBackFunId id)
        -> void
    {
        auto _ = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>(mtx_);
        callbacks_.erase(id);
    }

    auto ClearListener()
    {
        auto _ = Cot::LockGuard<RWMutexType, Cot::WriteLockTag>(mtx_);
        callbacks_.clear();
    }

    auto GetListener(CallBackFunId id)
        -> OnChangeCallback
    {
        return callbacks_.contains(id) ? callbacks_[id] : nullptr;
    }

private:
    RWMutexType mtx_;
    T val_;
    std::map<CallBackFunId, OnChangeCallback> callbacks_;
};
} //namespace ConfigT