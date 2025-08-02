#pragma once

#include <boost/lexical_cast.hpp>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/yaml.h>
namespace UtilT{
template <typename From, typename To>
class LexicalCast {
public:
    auto operator()(const From& v)
        -> To
    {
        return boost::lexical_cast<To>(v);
    }
};

/**
 * @brief convert YAML string to std::vector<T>
 */
template <typename T>
class LexicalCast<std::string, std::vector<T>> {

    auto operator()(const std::string& yaml_str)
        -> std::vector<T>
    {
        auto nodes = YAML::Load(yaml_str);
        auto vec = std::vector<T>{};

        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&vec](const auto& node)
                        -> void {
                        auto sstream = std::stringstream{};
                        sstream << node;
                        vec.push_back(LexicalCast<std::string, T>(sstream.str()));
                      });
        return vec;
    }
};

template <typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
    auto operator()(const std::vector<T>& v)
        -> std::string
    {
        auto node = YAML::Node{YAML::NodeType::Sequence};
        std::for_each(v.begin(),
                      v.end(),
                      [&node](const auto& elem)
                        -> void {
                            node.push_back(YAML::Load(LexicalCast<T, std::string>(elem)));
                    });
        return ( std::stringstream{} << node ).str();
    }
};

/**
 * @brief convert YAML String to std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::list<T>> {
public:
    auto operator()(const std::string& yaml_str)
        -> std::list<T>
    {
        auto list = std::list<T> {};
        auto nodes = YAML::Load(yaml_str);
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&list](const auto& node)
                        -> void {
                            auto sstream = std::stringstream{};
                            sstream << node;
                            list.push_back(LexicalCast<std::string, T>(sstream.str()));
                      });
        return list;
    }
};

/**
 * @brief convert std::list<T> to YAML String
 */
template <typename T>
class LexicalCast<std::list<T>, std::string> {
public:
    auto operator()(const std::list<T> list)
        -> std::string
    {
        auto nodes = YAML::Node{YAML::NodeType::Sequence};
        std::for_each(list.begin(),
                      list.end(),
                      [&nodes](const auto& elem)
                        -> void {
                            nodes.push_back(YAML::Load(LexicalCast<T, std::string>(elem)));
                      });
        return (std::stringstream() << nodes).str();
    }
};

/**
 * @brief convert std::set<T> to YAML String
 */
template <typename T>
class LexicalCast<std::set<T>, std::string> {
    auto operator()(const std::set<T> set)
        -> std::string
    {
        auto nodes = YAML::Node{YAML::NodeType::Sequence};
        std::for_each(set.begin(),
                      set.end(),
                      [&nodes](const auto& elem)
                          -> void{
                            nodes.push_back(YAML::Load(LexicalCast<T, std::string>(elem)));
                      });
        return (std::stringstream{} << nodes).str();
    }
};

/**
 * @brief convert String to std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::set<T>> {
public:
    auto operator()(const std::string& yaml_str)
        -> std::string
    {
        auto nodes = YAML::Load(yaml_str);
        auto set = std::set<T>{};
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&set](const auto& node)
                        -> void {
                            auto sstream = std::stringstream{};
                            sstream << node;
                            set.push_back(LexicalCast<std::string, T>(sstream.str()));
                      });
        return set;
    }
};

/**
 * @brief convert std::set<T> to YAML String
 */
template <typename T>
class LexicalCast<std::unordered_set<T>, std::string> {
    auto operator()(const std::unordered_set<T> set)
        -> std::string
    {
        auto nodes = YAML::Node{YAML::NodeType::Sequence};
        
        std::for_each(set.begin(),
                      set.end(),
                      [&nodes](const auto& elem)
                          -> void{
                            nodes.push_back(YAML::Load(LexicalCast<T, std::string>(elem)));
                      });
        return (std::stringstream{} << nodes).str();
    }
};

/**
 * @brief convert String to std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::unordered_set<T>> {
public:
    auto operator()(const std::string& yaml_str)
        -> std::string
    {
        auto nodes = YAML::Load(yaml_str);
        auto set = std::unordered_set<T>{};
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&set](const auto& node)
                        -> void {
                            auto sstream = std::stringstream{};
                            sstream << node;
                            set.insert(LexicalCast<std::string, T>(sstream.str()));
                      });
        return set;
    }
};

/**
 * @brief convert std::set<T> to YAML String
 */
template <typename T>
class LexicalCast<std::map<std::string, T>, std::string> {
    auto operator()(const std::map<std::string, T> map)
        -> std::string
    {
        auto nodes = YAML::Node{YAML::NodeType::Map};
        std::for_each(map.begin(),
                      map.end(),
                      [&nodes](const auto& elem)
                          -> void{
                            const auto &[key, value] = elem;
                            nodes[key] = YAML::Load(LexicalCast<T, std::string>(value));
                      });
        return (std::stringstream{} << nodes).str();
    }
};

/**
 * @brief convert String to std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::map<std::string, T>> {
public:
    auto operator()(const std::string& yaml_str)
        -> std::string
    {
        auto nodes = YAML::Load(yaml_str);
        auto map = std::map<std::string ,T>{};
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&map](const auto& node)
                        -> void {
                            auto sstream = std::stringstream{};
                            sstream << node->second;
                            map.insert(std::make_pair(node->first.Scalar(),
                                                      LexicalCast<std::string, T>(sstream.str())));
                      });
        return map;
    }
};

/**
 * @brief convert std::set<T> to YAML String
 */
template <typename T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
    auto operator()(const std::unordered_map<std::string, T> umap)
        -> std::string
    {
        auto nodes = YAML::Node{YAML::NodeType::Map};
        
        std::for_each(umap.begin(),
                      umap.end(),
                      [&nodes](const auto& elem)
                          -> void{
                            const auto& [key, value] = elem;

                            nodes[key] = YAML::Load(LexicalCast<T, std::string>(value));
                      });
        return (std::stringstream{} << nodes).str();
    }
};

/**
 * @brief convert String to std::list<T>
 */
template <typename T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
public:
    auto operator()(const std::string& yaml_str)
        -> std::unordered_map<std::string, T>
    {
        auto nodes = YAML::Load(yaml_str);
        auto umap = std::unordered_map<std::string, T>{};
        std::for_each(nodes.begin(),
                      nodes.end(),
                      [&umap](const auto& node)
                          -> void {
                          auto sstream = std::stringstream {};
                          sstream << node->second;
                            umap.insert(std::make_pair( node->first.Scalar(), LexicalCast<std::string, T>(sstream.str())));
                      });
        return umap;
    }
};

} //namespace UtilT