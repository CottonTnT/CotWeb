#include "config/config.h"

#include "envvar/envutils.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"

#include "mutex.h"
#include "util.hpp"
#include "utils/fsutil.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <sys/stat.h>
#include <unordered_map>

namespace ConfigT{

static auto s_RootLogger = GET_ROOT_LOGGER();

static auto ListAllMember(const std::string& prefix,
                          const YAML::Node& nodes,
                          std::list<std::pair<std::string, const YAML::Node>>& output)
{
    if (prefix.find_first_not_of(Config::c_ValidChars) != std::string::npos)
    {
        //todo log here;
        return;
    }
    output.emplace_back(prefix, nodes);
    if (nodes.IsMap())
    {
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&prefix, &output](const auto& node)
                          -> void { ListAllMember(prefix.empty() ? node.first.Scalar() : prefix + "." + node.first.Scalar(),
                                                  node,
                                                  output);
            }
        );
    }
}

auto Config::LoadFromYaml(const YAML::Node& root)
    -> void
{
    auto all_nodes = std::list<std::pair<std::string, const YAML::Node>>{};
    ListAllMember("", root, all_nodes);

    std::for_each(all_nodes.begin(),
                  all_nodes.end(),
                  [](const auto& node) 
                    -> void {

                    auto key = node.first;
                    if(key.empty()) return;

                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

                    auto var = LookupBase(key);

                    if (not var)
                        return;

                    if (node.second.IsScalar())
                    {
                        var->FromString(node.second.Scalar());
                    }
                    else
                    {
                        var->FromString((std::stringstream{} << node.second).str());
                    }
                  }
    );

}

/**
 * @brief for keep the every file`s last modify time
 */
static auto s_FileLastModifyTime = std::map<std::string, uint64_t>{};

static auto s_Mtx = Cot::Mutex{};


auto Config::LoadFromConfDir(const std::string& path, bool force)
    -> void
{
    auto abs_path = Env::Utils::getAbsolutePath(path);
    auto files = std::vector<std::string>{};
    Utils::FSUtils::ListAllFile(files, abs_path, ".yml");

    for (const auto& file : files)
    {
        {
            struct stat st;
            auto sucess = UtilT::SyscallWrapper<-1>(lstat, file.c_str(), &st);

            if (not sucess)
            {
                COT_LOG_ALERT(s_RootLogger) << sucess.error();
                continue;
            }
            
            auto _ = Cot::LockGuard<Cot::Mutex>{s_Mtx};
            if (not force and s_FileLastModifyTime[file] == static_cast<uint64_t>(st.st_mtime))
                continue;
            s_FileLastModifyTime[file] = st.st_mtime;
        }

        try
        {
            auto root_node = YAML::LoadFile(file);
            LoadFromYaml(root_node);
            COT_LOG_INFO(s_RootLogger) << "LoadConfigFile: file '" << file << "' OK";
        } catch (...)
        {
            COT_LOG_ERROR(s_RootLogger) << "LoadConfigFile:: file '" << file << "' FAILED";
        }
    }
    
}


auto Config::LookupBase(const std::string& name)
    -> Sptr<ConfigVarBase>
{
    auto _ = Cot::LockGuard<RWMutexType, Cot::ReadLockTag>{GetMutex_()};

    if (GetDatas_().contains(name))
    {
        return GetDatas_().at(name);
    }
    return nullptr;
}

auto Config::Visit(std::function<void(Sptr<ConfigVarBase>)> callback)
    -> void
{
    auto _ = Cot::LockGuard<RWMutexType, Cot::ReadLockTag>{GetMutex_()};
    auto& map = GetDatas_();
    std::for_each(map.begin(),
                  map.end(),
                  [cb = std::move(callback)](const auto& elem)
                      -> void {
                        cb(elem.second);
                  });
}

} //namespace ConfigT

