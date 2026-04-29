CXX        ?= g++
NVCC       ?= nvcc
NVCXX      ?= nvc++

BASEFLAGS  ?= -O3 -std=c++17 -Wall -Wextra
OMPFLAGS   ?= -fopenmp
NVCCFLAGS  ?= -O3 -std=c++17 -arch=$(CUDA_ARCH) -Xcompiler=-Wall,-Wextra
ACCFLAGS   ?= -O3 -std=c++17 -acc -gpu=$(ACC_GPU)

CUDA_ARCH  ?= sm_80
ACC_GPU    ?= cc80

.PHONY: cpu gpu all serial omp acc cuda my_omp my_acc clean help

cpu: stencil_serial stencil_omp

gpu: stencil_acc stencil_cuda

all: cpu gpu

serial: stencil_serial

omp: stencil_omp

acc: stencil_acc

cuda: stencil_cuda

my_omp: my_stencil_omp

my_acc: my_stencil_acc

help:
	@echo "Targets:"
	@echo "  make cpu          Build stencil_serial and stencil_omp"
	@echo "  make gpu          Build stencil_acc and stencil_cuda"
	@echo "  make all          Build all four executables"
	@echo "  make serial       Build the provided serial reference"
	@echo "  make omp          Build the OpenMP starter"
	@echo "  make acc          Build the OpenACC starter"
	@echo "  make cuda         Build the CUDA starter from stencil.cpp"
	@echo "  make my_omp       Build my_stencil_omp from my_stencil_omp.cpp"
	@echo "  make my_acc       Build my_stencil_acc from my_stencil_acc.cpp"
	@echo "  make clean        Remove executables"
	@echo ""
	@echo "Optional overrides:"
	@echo "  make cuda CUDA_ARCH=sm_80"
	@echo "  make acc ACC_GPU=cc80"

stencil_serial: stencil.cpp
	$(CXX) $(BASEFLAGS) -o $@ stencil.cpp

stencil_omp: stencil.cpp
	$(CXX) $(BASEFLAGS) $(OMPFLAGS) -DSTENCIL_USE_OPENMP -o $@ stencil.cpp

stencil_acc: stencil.cpp
	@command -v $(NVCXX) >/dev/null 2>&1 || { echo "Error: nvc++ not found in PATH"; exit 1; }
	$(NVCXX) $(ACCFLAGS) -DSTENCIL_USE_OPENACC -o $@ stencil.cpp

stencil_cuda: stencil.cpp
	@command -v $(NVCC) >/dev/null 2>&1 || { echo "Error: nvcc not found in PATH"; exit 1; }
	$(NVCC) -x cu $(NVCCFLAGS) -DSTENCIL_USE_CUDA -o $@ stencil.cpp

my_stencil_omp: my_stencil_omp.cpp
	$(CXX) $(BASEFLAGS) $(OMPFLAGS) -DSTENCIL_USE_OPENMP -o $@ my_stencil_omp.cpp

my_stencil_acc: my_stencil_acc.cpp
	@command -v $(NVCXX) >/dev/null 2>&1 || { echo "Error: nvc++ not found in PATH"; exit 1; }
	$(NVCXX) $(ACCFLAGS) -Mnofma -DSTENCIL_USE_OPENACC -o $@ my_stencil_acc.cpp

clean:
	rm -f stencil_serial stencil_omp stencil_acc stencil_cuda my_stencil_omp my_stencil_acc
