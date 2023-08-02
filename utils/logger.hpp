#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <fmt/format.h>

namespace console
{
  namespace internal
  {
    std::string getCurrentTimestamp()
    {
      auto now = std::chrono::system_clock::now();
      auto itt = std::chrono::system_clock::to_time_t(now);
      auto ittms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;

      std::stringstream ss;
      ss << std::put_time(std::localtime(&itt), "%Y%m%d %H:%M:%S");
      ss << '.' << std::setw(6) << std::setfill('0') << ittms.count();

      return ss.str();
    }

    inline const char *extract_filename(const char *path)
    {
      const char *filename = strrchr(path, '/');
#ifdef _WIN32
      if (!filename)
        filename = strrchr(path, '\\');
#endif
      return filename ? filename + 1 : path;
    }

    inline thread_local std::string currentDevice = "Unknown";

    void setDeviceString(const std::string &str)
    {
      currentDevice = str;
    }

    template <typename... Args>
    void log(std::string level, const char *file, const int line, std::string message, Args &&...args)
    {
      auto timestamp = getCurrentTimestamp();

      auto messageFormatted = fmt::format(message, args...);

      fmt::println("[{}] [{}] [{}] {} - {}:{}", timestamp, currentDevice, level, messageFormatted, extract_filename(file), line);
    }
  }

#define info(msg, ...) console::internal::log("INFO", __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define warn(msg, ...) console::internal::log("WARNING", __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define error(msg, ...) console::internal::log("ERROR", __FILE__, __LINE__, msg, ##__VA_ARGS__)
}
