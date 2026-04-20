# 2D Stencil Starter Code (Single Source, Shared Loop Nest)

This starter package uses **one source file**:

- `stencil.cpp`

The same file is compiled into four executables with different build flags:

- `stencil_serial`
- `stencil_omp`
- `stencil_acc`
- `stencil_cuda`

## Build

### CPU builds

```bash
make cpu
```

This builds:

- `stencil_serial`
- `stencil_omp`

### Individual CPU builds

```bash
make serial
make omp
```

### OpenACC build

```bash
make acc
```

### CUDA build

```bash
make cuda
```

### Build all four executables

```bash
make all
```

## Output format

Each executable prints `key=value` lines. Example:

```text
program=stencil_serial
nx=1024
ny=1024
steps=100
runtime_seconds=0.123456
checksum=102400.0000000000
```

The OpenMP version also prints `threads=...`.

The OpenACC version prints:

- `compute_seconds`
- `end_to_end_seconds`

The CUDA version prints:

- `block_x`
- `block_y`
- `compute_seconds`
- `end_to_end_seconds`
