# tma-scope
Allow to profile a function with the TMAM (Top Down Analysis Methodology) thanks to DBI (Dynamic Binary Instrumentation).

# Requirements
 - QDBI (https://github.com/QBDI/QBDI)
 - libelf (Arch: libelf, Debian: libelf-dev)

See the QBDI documentation for more information.

# Build
```bash
cmake -B build
make -C build -j 8
```

# Usage

- `TMA_FUNCTION` is the function to profile. Note that this must be the demangled name of the function if it is a C++ function.
- `TMA_OUTPUT_FILE` is the file where the output of perf will be written.
- `TMA_LEVEL` is the level of the TMA analysis. 
 - **Perf** It can be `TopdownL1`, `TopdownL2`, `TopdownL3`, `TopdownL3`.
 - **Toplev**: `1` for L1, `2` for L2, `3` for L3
- `TMA_TOPLEV` controls whether to use the toplev tool or not. The value of the argument will be the path to the `toplev` tool.

Note that sampling tools require a more-or-less large number of samples to be accurate. Hence, you should repeat your workload enough times to get a meaningful output.

For a simple example using `perf`:
```bash
TMA_FUNCTION=kernel TMA_OUTPUT_FILE=tma.txt TMA_LEVEL=TopdownL1 LD_PRELOAD=./libtmascope.so {YOUR_PROGRAM}
```

Same example using `toplev` considering that `pmu-tools` is the path to the `toplev` tool:
```bash
TMA_FUNCTION=kernel TMA_OUTPUT_FILE=toplev3.txt TMA_LEVEL=1 TMA_TOPLEV=pmu-tools LD_PRELOAD=./libtmascope.so {YOUR_PROGRAM}
```