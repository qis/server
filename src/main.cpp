#include <app/config.hpp>
#include <net/server.hpp>
#include <spdlog/async.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <cstdlib>

#define LOG_PATTERN_DEBUG "[%T.%e] [%^%L%$] %v %@"
#define LOG_PATTERN_RELEASE "[%Y-%m-%d %T.%e] [%^%L%$] %v"

#ifndef NDEBUG
#define LOG_PATTERN LOG_PATTERN_DEBUG
#else
#define LOG_PATTERN LOG_PATTERN_RELEASE
#endif

namespace {

auto application(std::error_code& ec) noexcept -> std::filesystem::path
{
  ec.clear();
  std::filesystem::path path;
#ifdef _WIN32
  std::string s;
  DWORD size = 0;
  DWORD code = 0;
  do {
    s.resize(s.size() + MAX_PATH);
    size = GetModuleFileName(nullptr, s.data(), static_cast<DWORD>(s.size()));
    code = GetLastError();
  } while (code == ERROR_INSUFFICIENT_BUFFER);
  if (code) {
    ec = std::error_code(static_cast<int>(code), std::system_category());
    return {};
  }
  s.resize(size);
  path = std::filesystem::path(s);
#else
  path = "/proc/self/exe";
#endif
  return std::filesystem::canonical(path, ec);
}

std::filesystem::path application()
{
  std::error_code ec;
  const auto result = application(ec);
  if (ec) {
    throw std::system_error(ec, "Could not determine application path");
  }
  return result;
}

struct daily_filename_calculator {
  static auto calc_filename(const spdlog::filename_t& filename, const tm& now_tm) -> spdlog::filename_t
  {
    auto [basename, ext] = spdlog::details::file_helper::split_by_extension(filename);
    return fmt::format("{}-{:04d}-{:02d}-{:02d}{}", basename, now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, ext);
  }
};

class sink : public spdlog::sinks::sink {
public:
  sink(spdlog::color_mode mode = spdlog::color_mode::automatic)
  {
    styles_[spdlog::level::trace] = fmt::fg(fmt::terminal_color::bright_black) | fmt::emphasis::bold;
    styles_[spdlog::level::debug] = fmt::fg(fmt::terminal_color::bright_white) | fmt::emphasis::bold;
    styles_[spdlog::level::info] = fmt::fg(fmt::terminal_color::bright_green) | fmt::emphasis::bold;
    styles_[spdlog::level::warn] = fmt::fg(fmt::terminal_color::bright_yellow) | fmt::emphasis::bold;
    styles_[spdlog::level::err] = fmt::fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold;
    styles_[spdlog::level::critical] = fmt::fg(fmt::terminal_color::bright_magenta) | fmt::emphasis::bold;
#ifdef _WIN32
    handle_ = ::GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    in_console_ = ::GetConsoleMode(handle_, &console_mode) != 0;
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    ::GetConsoleScreenBufferInfo(handle_, &info);
    colors_[spdlog::level::trace] = FOREGROUND_INTENSITY;
    colors_[spdlog::level::debug] = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    colors_[spdlog::level::info] = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
    colors_[spdlog::level::warn] = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
    colors_[spdlog::level::err] = FOREGROUND_INTENSITY | FOREGROUND_RED;
    colors_[spdlog::level::critical] = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE;
    colors_[spdlog::level::off] = info.wAttributes;
#endif
    set_color_mode(mode);
  }

  sink(const sink& other) = delete;
  sink& operator=(const sink& other) = delete;

  ~sink() override = default;

  void log(const spdlog::details::log_msg& cmsg) override
  {
#ifdef _WIN32
    constexpr auto prefix = ".\\";
#else
    constexpr auto prefix = "./";
#endif
    spdlog::memory_buf_t formatted;
#ifndef NDEBUG
    auto msg = cmsg;
    std::string filename;
    if (msg.source.filename) {
      filename = msg.source.filename;
      if (const auto pos = filename.find("src"); pos != std::string::npos && pos > 3) {
        filename[pos - 2] = prefix[0];
        filename[pos - 1] = prefix[1];
        msg.source.filename = filename.data() + pos - 2;
      }
    }
#else
    const auto& msg = cmsg;
#endif
    std::lock_guard lock{ mutex_ };
    formatter_->format(msg, formatted);
    if (should_color_ && msg.level < styles_.size() && msg.color_range_end > msg.color_range_start) {
      print_range(formatted, 0, msg.color_range_start);
      print_range(formatted, msg.color_range_start, msg.color_range_end, msg.level);
#ifndef NDEBUG
      const auto str = std::string_view{ formatted.data() + msg.color_range_end, formatted.size() - msg.color_range_end };
      const auto pos = str.rfind(prefix) + msg.color_range_end;
      print_range(formatted, msg.color_range_end, pos);
      print_range(formatted, pos, formatted.size(), spdlog::level::trace);
#else
      print_range(formatted, msg.color_range_end, formatted.size());
#endif
    } else {
      print_range(formatted, 0, formatted.size());
    }
    fflush(stdout);
  }

  void set_pattern(const std::string& pattern) final
  {
    std::lock_guard lock{ mutex_ };
    formatter_ = std::make_unique<spdlog::pattern_formatter>(pattern);
  }

  void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override
  {
    std::lock_guard lock{ mutex_ };
    formatter_ = std::move(formatter);
  }

  void flush() override
  {
    std::lock_guard lock{ mutex_ };
    fflush(stdout);
  }

  void set_color_mode(spdlog::color_mode mode)
  {
    std::lock_guard lock{ mutex_ };
    switch (mode) {
    case spdlog::color_mode::always:
      should_color_ = true;
      break;
    case spdlog::color_mode::automatic:
#if _WIN32
      should_color_ = spdlog::details::os::in_terminal(stdout) || IsDebuggerPresent();
#else
      should_color_ = spdlog::details::os::in_terminal(stdout) && spdlog::details::os::is_color_terminal();
#endif
      break;
    case spdlog::color_mode::never:
      should_color_ = false;
      break;
    }
  }

private:
  void print_range(const spdlog::memory_buf_t& formatted, size_t start, size_t end)
  {
#ifdef _WIN32
    if (in_console_) {
      auto data = formatted.data() + start;
      auto size = static_cast<DWORD>(end - start);
      while (size > 0) {
        DWORD written = 0;
        if (!::WriteFile(handle_, data, size, &written, nullptr) || written == 0 || written > size) {
          SPDLOG_THROW(spdlog::spdlog_ex("sink: print_range failed. GetLastError(): " + std::to_string(::GetLastError())));
        }
        size -= written;
      }
      return;
    }
#endif
    fwrite(formatted.data() + start, sizeof(char), end - start, stdout);
  }

  void print_range(const spdlog::memory_buf_t& formatted, size_t start, size_t end, spdlog::level::level_enum level)
  {
#ifdef _WIN32
    if (in_console_) {
      ::SetConsoleTextAttribute(handle_, colors_[level]);
      print_range(formatted, start, end);
      ::SetConsoleTextAttribute(handle_, colors_[spdlog::level::off]);
      return;
    }
#endif
    fmt::print(stdout, styles_[level], "{}", std::string_view{ formatted.data() + start, end - start });
  }

  std::mutex mutex_;
  std::unique_ptr<spdlog::formatter> formatter_;
  std::array<fmt::text_style, spdlog::level::off> styles_;
#ifdef _WIN32
  std::array<WORD, 7> colors_;
  bool in_console_ = true;
  HANDLE handle_ = nullptr;
#endif
  bool should_color_ = false;
};

void logger(spdlog::level::level_enum severity, std::optional<std::filesystem::path> file = {}, std::uint16_t max = 1)
{
  spdlog::init_thread_pool(8192, 1);
  spdlog::default_logger()->sinks().clear();
  spdlog::default_logger()->sinks().push_back(std::make_shared<sink>());
  spdlog::set_pattern(LOG_PATTERN, spdlog::pattern_time_type::local);
  if (file) {
    if (max > 1) {
      using sink_type = spdlog::sinks::daily_file_sink<std::mutex, daily_filename_calculator>;
      auto sink = std::make_shared<sink_type>(file->string(), 0, 0, false, max);
      spdlog::default_logger()->sinks().push_back(sink);
      sink->set_pattern(LOG_PATTERN_RELEASE);
    } else {
      using sink_type = spdlog::sinks::basic_file_sink<std::mutex>;
      auto sink = std::make_shared<sink_type>(file->string(), max == 0);
      spdlog::default_logger()->sinks().push_back(sink);
      sink->set_pattern(LOG_PATTERN_RELEASE);
    }
  }
  spdlog::set_level(severity);
  spdlog::flush_on(severity);
}

}  // namespace

int main(int argc, char* argv[])
{
  app::config config;
  std::filesystem::path data;
  std::filesystem::path html;
  try {
    std::filesystem::path file;
    std::filesystem::path path = application().parent_path().parent_path();
    if (argc > 1) {
      file = std::filesystem::absolute(std::filesystem::path(argv[1]));
    } else {
      file = std::filesystem::absolute(path / "etc" / "server.ini");
    }
    if (argc > 2) {
      html = std::filesystem::absolute(std::filesystem::path(argv[2]));
    } else {
      html = path / "html";
    }
    if (argc > 3) {
      data = std::filesystem::absolute(std::filesystem::path(argv[3]));
    } else {
      data = path / "html" / "data";
    }
    config.parse(file);
    logger(config.log.severity, config.log.filename, 0);
  }
  catch (const std::system_error& e) {
    fmt::print(stderr, "{}: {} ({})\n", e.code().category().name(), e.what(), e.code().value());
    return e.code().value();
  }
  catch (const std::exception& e) {
    fmt::print(stderr, "error: {}\n", e.what());
    return EXIT_FAILURE;
  }
  try {
    asio::io_context context{ 1 };
    asio::signal_set signals(context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) {
      context.stop();
    });
    asio::co_spawn(context.get_executor(), net::server{ std::move(config), html, data }, asio::detached);
    context.run();
  }
  catch (const boost::system::system_error& e) {
    LOGC("{}: {} ({})", e.code().category().name(), e.what(), e.code().value());
    return e.code().value();
  }
  catch (const std::system_error& e) {
    LOGC("{}: {} ({})", e.code().category().name(), e.what(), e.code().value());
    return e.code().value();
  }
  catch (const std::exception& e) {
    LOGC("{}", e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
