#pragma once

#include <filesystem>

// Basically a global variable
class Logger {
private:
#ifdef GLYNTH_LOG_DIR
  inline static auto s_log_dir = std::filesystem::path(GLYNTH_LOG_DIR);
  inline static auto s_log_file = std::filesystem::path(s_log_dir) / "logs.txt";
#endif

public:
#ifdef GLYNTH_LOG_TO_FILE
  inline static FILE* file = fopen(s_log_file.c_str(), "a");
#else
  inline static FILE* file = stdout;
#endif
};
