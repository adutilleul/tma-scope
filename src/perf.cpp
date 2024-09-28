#include "QBDI.h"
#include "QBDIPreload.h"
#include "elf.hpp"
#include "proc.hpp"

#include <iostream>
#include <optional>
#include <cstddef>
#include <algorithm>

#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

struct stack_s
{
  QBDI::rword returnAddress;
};

/**
 * @brief Get the address of the the symbol in the specified process
 *
 * @param pid Process ID
 * @param name Unmangled symbol name
 * @return Memory address (**Note: this is sensitive because it would allow an attacker to bypass
 * ASLR if leaked!**)
 */
static QBDI::rword getAddress(pid_t pid, std::string name)
{
  // Get the executable path
  std::string path = getPath(pid);

  // Get all symbols
  std::vector symbols = getSymbols(path);

  // Get the symbol address
  QBDI::rword symbolAddress = 0;
  QBDI::rword symbolEndAddress = 0;
  for (Symbol symbol : symbols)
  {
    if (symbol.unmangledName == name)
    {
      symbolAddress = symbol.address;
      symbolEndAddress = symbol.address + symbol.size;
      break;
    }
  }

  if (symbolAddress == 0)
  {
    throw std::runtime_error("Failed to get symbol address!");
  }

  // Get all memory map entries
  std::vector<QBDI::MemoryMap> entries = QBDI::getCurrentProcessMaps(true);

  // Get the base address
  QBDI::rword baseAddress = 0;
  for (QBDI::MemoryMap entry : entries)
  {
    // Check if the paths match
    if (entry.name == path && (entry.permission & QBDI::PF_EXEC) == 0)
    {
      baseAddress = entry.range.start();
      break;
    }
  }

  if (baseAddress == 0)
  {
    throw std::runtime_error("Failed to get base address!");
  }

  // Compute the full address
  QBDI::rword fullAddress = baseAddress + symbolAddress;

  return fullAddress;
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

static QBDI::VMAction onTargetEnd(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data)
{
  return QBDI::STOP;
}

static QBDI::VMAction onTarget(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState, QBDI::FPRState *fprState, void *data)
{
  #if defined(QBDI_ARCH_X86_64)
  stack_s* stack = reinterpret_cast<stack_s*>(gprState->rsp);
  vm->addCodeAddrCB((QBDI::rword)stack->returnAddress, QBDI::PREINST, onTargetEnd, nullptr);
  #endif

  pid_t pid = getpid();

  vfork_child_run({"/usr/bin/taskset", "--pid", "--cpu-list", "0", std::to_string(pid)}, [&]() {
    std::string tma_output_file = getenv("TMA_OUTPUT_FILE");
    std::string tma_level = getenv("TMA_LEVEL");
    vfork_child_run({"/usr/bin/perf", "stat", "-M", tma_level, "-p", std::to_string(pid), "-o", tma_output_file});
  });

  return QBDI::CONTINUE;
}

extern "C"
{
  /**
   * @brief QBDIPreload constructor
   *
   */
  QBDIPRELOAD_INIT;

  /**
   * @brief QBDIPreload program entrypoint handler
   *
   * @param main Main function address
   * @return QBDIpreload state
   */
  int qbdipreload_on_start(void *main)
  {
    return QBDIPRELOAD_NOT_HANDLED;
  }

  /**
   * @brief QBDIPreload main function preload handler
   *
   * @param gprCtx Original General Purpose Registers (GPR) context
   * @param fpuCtx Original Floating Point Registers (FPR) context
   * @return QBDIpreload state
   */
  int qbdipreload_on_premain(void *gprCtx, void *fprCtx)
  {
    return QBDIPRELOAD_NOT_HANDLED;
  }

  /**
   * @brief QBDIPreload main function hijacked handler
   *
   * @param argc Argument count
   * @param argv Argument values
   * @return QBDIpreload state
   */
  int qbdipreload_on_main(int argc, char *argv[])
  {
    return QBDIPRELOAD_NOT_HANDLED;
  }

  /**
   * @brief QBDIPreload main function ready-to-run handler
   *
   * @param vm QBDIPreload virtual machine
   * @param start Start address
   * @param stop Stop address
   * @return QBDIpreload state
   */
  int qbdipreload_on_run(QBDI::VMInstanceRef vm, QBDI::rword start, QBDI::rword stop)
  {
    // Get own pid
    pid_t pid = getpid();

    const char *tma_function_name = getenv("TMA_FUNCTION");
    if (tma_function_name == NULL)
    {
      printf("TMA_FUNCTION not set\n");
      return QBDIPRELOAD_ERR_STARTUP_FAILED;
    }

    const char *tma_output_file = getenv("TMA_OUTPUT_FILE");
    if (tma_output_file == NULL)
    {
      printf("TMA_OUTPUT_FILE not set\n");
      return QBDIPRELOAD_ERR_STARTUP_FAILED;
    }

    const char *tma_level = getenv("TMA_LEVEL");
    if (tma_level == NULL)
    {
      printf("TMA_LEVEL not set\n");
      return QBDIPRELOAD_ERR_STARTUP_FAILED;
    }

    // //Get the target address
    auto targetAddress = getAddress(pid, tma_function_name);

    // Register callbacks
    vm->addCodeAddrCB((QBDI::rword)targetAddress, QBDI::PREINST, onTarget, NULL);

    // Run
    vm->run(start, stop);

    return QBDIPRELOAD_NO_ERROR;
  }

  /**
   * @brief QBDIPreload program exit handler handler
   *
   * @param status Exit status
   * @return QBDIpreload state
   */
  int qbdipreload_on_exit(int status)
  {
    return QBDIPRELOAD_NO_ERROR;
  }
}