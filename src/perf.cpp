#include "dr_api.h"
#include "dr_events.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"

#include <iostream>
#include <optional>
#include <cstddef>
#include <algorithm>
#include <vector>
#include <functional>

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

//Imports
#include "dr_api.h"
#include "dr_events.h"
#include "drmgr.h"
#include "drsyms.h"
#include "drwrap.h"
#include <stdint.h>

/**
 * @brief Target function post-execution handler
 * 
 * @param ctx DynamoRIO context
 * @param data User data
 */
static void onAfterTarget(void *ctx, void *data)
{
}

void child_run(const std::vector<std::string>& cmd) {
    const char** args = new const char*[cmd.size() + 1];
    std::transform(cmd.begin(), cmd.end(), args, [](const std::string& arg) {
        return arg.c_str();
    });
    args[cmd.size()] = nullptr;
    execve(args[0], (char* const*)args, nullptr);
}

void vfork_child_run(const std::vector<std::string>& cmd, std::optional<std::function<void()>> before_exec = std::nullopt) {
    if (before_exec.has_value()) {
        before_exec.value()();
    }

    pid_t pid = vfork();
    if (pid == 0) {
        child_run(cmd);
    }
}

/**
 * @brief Target function pre-execution handler
 * 
 * @param ctx DynamoRIO context
 * @param data User data
 */
static void onBeforeTarget(void *ctx, void **data)
{
  pid_t pid = getpid();

  dr_printf("Before target: %d\n", pid);

  std::string tma_core = getenv("TMA_CORE");

  vfork_child_run({"/usr/bin/taskset", "--pid", "--cpu-list", "0", std::to_string(pid)}, [&]() {
    std::string tma_output_file = getenv("TMA_OUTPUT_FILE");
    std::string tma_level = getenv("TMA_LEVEL");
    if(getenv("TMA_TOPLEV") == NULL) {
      vfork_child_run({"/usr/bin/perf", "stat", "-M", tma_level, "-p", std::to_string(pid), "-o", tma_output_file});
    } else {
      std::string tma_toplev = getenv("TMA_TOPLEV");
      vfork_child_run({"/usr/bin/python", tma_toplev + "/toplev.py", "--single-thread", "-l", tma_level, "-o", tma_output_file, "--pid", std::to_string(pid), "--core", "C"+tma_core});
    }

  });
}

/**
 * @brief DynamoRIO module loaded handler
 * 
 * @param ctx DynamoRIO context
 * @param module Module that was loaded
 * @param loaded Whether or not the module was loaded?
 */
static void onModuleLoad(void *ctx, const module_data_t *module, bool loaded)
{
  const char *tma_function_name = getenv("TMA_FUNCTION");
  if (tma_function_name == NULL)
  {
    printf("TMA_FUNCTION not set\n");
    exit(1);
  }

  const char *tma_core = getenv("TMA_CORE");
  if (tma_core == NULL)
  {
    printf("TMA_CORE not set\n");
    exit(1);
  }

  const char *tma_output_file = getenv("TMA_OUTPUT_FILE");
  if (tma_output_file == NULL)
  {
    printf("TMA_OUTPUT_FILE not set\n");
    exit(1);
  }

  const char *tma_level = getenv("TMA_LEVEL");
  if (tma_level == NULL)
  {
    printf("TMA_LEVEL not set\n");
    exit(1);
  }

  //Get the target function offset
  size_t offset;
  drsym_error_t error = drsym_lookup_symbol(module->full_path, tma_function_name, &offset, DRSYM_LEAVE_MANGLED);

  //Get the target function address
  app_pc address = module->start + offset;

  if (error == DRSYM_SUCCESS)
  {
    printf("Symbol found: %s\n", tma_function_name);
    //Wrap the target function
    drwrap_wrap(address, onBeforeTarget, onAfterTarget);
  } else {
    printf("Symbol not found: %s\n", tma_function_name);
  }
}

/**
 * @brief DynamoRIO exit handler
 */
static void onExit()
{
  //Cleanup
  drsym_exit();
  drwrap_exit();
  drmgr_exit();
}

/**
 * @brief DynamoRIO entry point
 * 
 * @param id Client ID
 * @param argc Argument count
 * @param argv Argument values
 */
DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[])
{
  //Update client metadata
  dr_set_client_name("tmascope", "https://github.com/adutilleul/tma-scope");

  //Initialize DynamoRIO
  drmgr_init();
  drwrap_init();
  drsym_init(0);

  //Register handlers
  dr_register_exit_event(onExit);
  drmgr_register_module_load_event(onModuleLoad);

  //Configure function wrapping (Improve performance)
  drwrap_set_global_flags(static_cast<drwrap_global_flags_t>(DRWRAP_NO_FRILLS | DRWRAP_FAST_CLEANCALLS));
}