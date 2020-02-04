#include "config.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <sstream>

namespace boost::property_tree {

template <>
struct translator_between<std::string, asio::ip::address> {
  struct type {
    using internal_type = std::string;
    using external_type = asio::ip::address;
    static auto get_value(const std::string& value) -> boost::optional<external_type>
    {
      return asio::ip::address::from_string(value);
    }
  };
};

template <>
struct translator_between<std::string, std::filesystem::path> {
  struct type {
    using internal_type = std::string;
    using external_type = std::filesystem::path;
    static auto get_value(const std::string& value) -> boost::optional<external_type>
    {
      return std::filesystem::path(value);
    }
  };
};

template <>
struct translator_between<std::string, spdlog::level::level_enum> {
  struct type {
    using internal_type = std::string;
    using external_type = spdlog::level::level_enum;
    static auto get_value(const std::string& value) -> boost::optional<external_type>
    {
      if (value == "trace") {
        return spdlog::level::trace;
      }
      if (value == "debug") {
        return spdlog::level::debug;
      }
      if (value == "info") {
        return spdlog::level::info;
      }
      if (value == "warn") {
        return spdlog::level::warn;
      }
      if (value == "err") {
        return spdlog::level::err;
      }
      if (value == "critical") {
        return spdlog::level::critical;
      }
      if (value == "off") {
        return spdlog::level::off;
      }
      throw std::runtime_error("Invalid severity (" + value + ")");
    }
  };
};

}  // namespace boost::property_tree

namespace app {

void config::parse(const std::filesystem::path& file)
{
  std::stringstream ss;
  std::ifstream is(file, std::ios::binary);
  if (!is) {
    throw std::runtime_error("Could not open file.");
  }
  for (std::string line; std::getline(is, line);) {
    if (const auto pos = line.find(';'); pos != std::string::npos) {
      line.erase(pos);
    }
    ss << line << '\n';
  }
  boost::property_tree::ptree pt;
  boost::property_tree::ini_parser::read_ini(ss, pt);
  server.address = pt.get<std::string>("server.address", "0.0.0.0");
  server.service = pt.get<std::string>("server.service", "8080");
  server.proxied = pt.get<bool>("server.proxied", false);
  if (pt.get_child_optional("log.filename")) {
    log.filename = pt.get<std::filesystem::path>("log.filename");
    if (log.filename->is_relative()) {
      log.filename = std::filesystem::absolute(file.parent_path() / *log.filename);
    }
  }
  log.severity = pt.get<spdlog::level::level_enum>("log.severity", log.severity);
}

}  // namespace app
