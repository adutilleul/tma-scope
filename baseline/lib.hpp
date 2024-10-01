#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void child_run(const std::vector<std::string> &cmd) {
  const char **args = new const char *[cmd.size() + 1];
  std::transform(cmd.begin(), cmd.end(), args,
                 [](const std::string &arg) { return arg.c_str(); });
  args[cmd.size()] = nullptr;
  execve(args[0], (char *const *)args, nullptr);
}

void vfork_child_run(
    const std::vector<std::string> &cmd,
    std::optional<std::function<void()>> before_exec = std::nullopt) {
  if (before_exec.has_value()) {
    before_exec.value()();
  }

  pid_t pid = vfork();
  if (pid == 0) {
    child_run(cmd);
  }
}

void lib_start_record() {
  pid_t pid = getpid();
  std::string tma_core = getenv("TMA_CORE");

  vfork_child_run(
      {"/usr/bin/taskset", "--pid", "--cpu-list", tma_core,
       std::to_string(pid)},
      [&]() {
        std::string tma_output_file = getenv("TMA_OUTPUT_FILE");
        std::string tma_level = getenv("TMA_LEVEL");
        if (getenv("TMA_TOPLEV") == NULL) {
          vfork_child_run({"/usr/bin/perf", "stat", "-M", tma_level, "-p",
                           std::to_string(pid), "-o", tma_output_file});
        } else {
          std::string tma_toplev = getenv("TMA_TOPLEV");
          vfork_child_run({"/usr/bin/python", tma_toplev + "/toplev.py",
                           "--single-thread", "-l", tma_level, "-o",
                           tma_output_file, "--pid", std::to_string(pid),
                           "--core", "C" + tma_core});
        }
      });
}

void lib_stop_record() {
  exit(0);
}

template <typename... Args>
void lib_record_fn(void (*func)(Args...), Args... args) {
  lib_start_record();
  func(args...);
  lib_stop_record();
}