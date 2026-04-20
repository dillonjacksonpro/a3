#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(STENCIL_USE_OPENMP)
#ifdef _OPENMP
#include <omp.h>
#endif
#endif

#if defined(STENCIL_USE_CUDA)
#ifndef __CUDACC__
#error "STENCIL_USE_CUDA requires compiling stencil.cpp with nvcc -x cu"
#endif
#include <cuda_runtime.h>
#endif

#if defined(STENCIL_USE_OPENMP) && (defined(STENCIL_USE_OPENACC) || defined(STENCIL_USE_CUDA))
#error "Select only one parallel model at a time"
#endif
#if defined(STENCIL_USE_OPENACC) && defined(STENCIL_USE_CUDA)
#error "Select only one parallel model at a time"
#endif

namespace stencil {

using real_t = float;

#if defined(__CUDACC__)
#define STENCIL_HD __host__ __device__
#else
#define STENCIL_HD
#endif

class Timer {
public:
    Timer() : start_(clock::now()) {}

    void reset() {
        start_ = clock::now();
    }

    double elapsed_seconds() const {
        return std::chrono::duration<double>(clock::now() - start_).count();
    }

private:
    using clock = std::chrono::steady_clock;
    clock::time_point start_;
};

struct BasicArgs {
    int nx;
    int ny;
    int steps;
};

struct ParsedArgs {
    BasicArgs basic;
    int threads = 1;
    int block_x = 16;
    int block_y = 16;
};

inline const char* program_name() {
#if defined(STENCIL_USE_CUDA)
    return "stencil_cuda";
#elif defined(STENCIL_USE_OPENACC)
    return "stencil_acc";
#elif defined(STENCIL_USE_OPENMP)
    return "stencil_omp";
#else
    return "stencil_serial";
#endif
}

inline std::size_t cell_count(int nx, int ny) {
    return static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny);
}

inline int parse_positive_int(const char* text, const std::string& name) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);

    if (text == end || *end != '\0') {
        throw std::invalid_argument(name + " must be an integer: '" + text + "'");
    }
    if (value <= 0 || value > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(name + " must be a positive integer: '" + text + "'");
    }
    return static_cast<int>(value);
}

inline BasicArgs parse_basic_args(int argc, char** argv, const std::string& usage_tail = "") {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " nx ny steps";
        if (!usage_tail.empty()) {
            std::cerr << ' ' << usage_tail;
        }
        std::cerr << "\n";
        std::exit(1);
    }

    BasicArgs args{};
    args.nx = parse_positive_int(argv[1], "nx");
    args.ny = parse_positive_int(argv[2], "ny");
    args.steps = parse_positive_int(argv[3], "steps");

    if (args.nx < 3 || args.ny < 3) {
        throw std::invalid_argument("nx and ny must both be at least 3");
    }

    return args;
}

inline ParsedArgs parse_args(int argc, char** argv) {
    ParsedArgs parsed{};

#if defined(STENCIL_USE_OPENMP)
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " nx ny steps threads\n";
        std::exit(1);
    }
    parsed.basic = parse_basic_args(argc, argv, "threads");
    parsed.threads = parse_positive_int(argv[4], "threads");
#elif defined(STENCIL_USE_CUDA)
    if (argc != 4 && argc != 6) {
        std::cerr << "Usage: " << argv[0] << " nx ny steps [block_x block_y]\n";
        std::exit(1);
    }
    parsed.basic = parse_basic_args(argc, argv, "[block_x block_y]");
    if (argc == 6) {
        parsed.block_x = parse_positive_int(argv[4], "block_x");
        parsed.block_y = parse_positive_int(argv[5], "block_y");
    }
#else
    parsed.basic = parse_basic_args(argc, argv);
#endif

    return parsed;
}

STENCIL_HD inline std::size_t idx(int i, int j, int ny) {
    return static_cast<std::size_t>(i) * static_cast<std::size_t>(ny) + static_cast<std::size_t>(j);
}

STENCIL_HD inline real_t stencil_value(const real_t* src, int i, int j, int ny) {
    return 0.2f * (
        src[idx(i, j, ny)] +
        src[idx(i - 1, j, ny)] +
        src[idx(i + 1, j, ny)] +
        src[idx(i, j - 1, ny)] +
        src[idx(i, j + 1, ny)]
    );
}

inline void initialize_hotspot(real_t* grid, int nx, int ny) {
    const std::size_t n = cell_count(nx, ny);
    std::fill(grid, grid + n, 0.0f);

    const int cx = nx / 2;
    const int cy = ny / 2;
    const int hx = std::max(1, nx / 8);
    const int hy = std::max(1, ny / 8);

    for (int i = 1; i < nx - 1; ++i) {
        for (int j = 1; j < ny - 1; ++j) {
            if (std::abs(i - cx) < hx && std::abs(j - cy) < hy) {
                grid[idx(i, j, ny)] = 100.0f;
            }
        }
    }
}

inline void stencil_step(const real_t* src, real_t* dst, int nx, int ny) {
    const std::size_t n = cell_count(nx, ny);
    (void)n;

#if defined(STENCIL_USE_OPENMP)
#elif defined(STENCIL_USE_OPENACC)
#endif
    for (int i = 1; i < nx - 1; ++i) {
        for (int j = 1; j < ny - 1; ++j) {
            dst[idx(i, j, ny)] = stencil_value(src, i, j, ny);
        }
    }
}

inline double checksum(const real_t* data, std::size_t n) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += static_cast<double>(data[i]);
    }
    return sum;
}

inline void print_common_header(int nx, int ny, int steps) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "program=" << program_name() << "\n";
    std::cout << "nx=" << nx << "\n";
    std::cout << "ny=" << ny << "\n";
    std::cout << "steps=" << steps << "\n";
}

} // namespace stencil

#if defined(STENCIL_USE_CUDA)
#define CUDA_CHECK(call)                                                          \
    do {                                                                          \
        const cudaError_t err__ = (call);                                         \
        if (err__ != cudaSuccess) {                                               \
            std::cerr << "CUDA error at " << __FILE__ << ':' << __LINE__        \
                      << ": " << cudaGetErrorString(err__) << "\n";           \
            return 1;                                                             \
        }                                                                         \
    } while (0)

#ifndef STENCIL_CUDA_STUB
#define STENCIL_CUDA_STUB 1
#endif

__global__ void stencil_step_kernel(const stencil::real_t* src,
                                    stencil::real_t* dst,
                                    int nx,
                                    int ny) {
#if STENCIL_CUDA_STUB
    (void)src;
    (void)dst;
    (void)nx;
    (void)ny;
#else
#endif
}
#endif

int main(int argc, char** argv) {
    try {
        const stencil::ParsedArgs args = stencil::parse_args(argc, argv);
        const int nx = args.basic.nx;
        const int ny = args.basic.ny;
        const int steps = args.basic.steps;
        const std::size_t n = stencil::cell_count(nx, ny);

#if defined(STENCIL_USE_CUDA)
#if STENCIL_CUDA_STUB
        stencil::print_common_header(nx, ny, steps);
        std::cout << "block_x=" << args.block_x << "\n";
        std::cout << "block_y=" << args.block_y << "\n";
        std::cout << "implementation_status=starter_stub\n";
        std::cerr << "The CUDA starter path is not implemented yet.\n";
        return 2;
#else
        std::vector<stencil::real_t> host_a(n, 0.0f);
        std::vector<stencil::real_t> host_b(n, 0.0f);
        stencil::initialize_hotspot(host_a.data(), nx, ny);

        stencil::real_t* d_a = nullptr;
        stencil::real_t* d_b = nullptr;
        const std::size_t bytes = n * sizeof(stencil::real_t);

        stencil::Timer end_to_end_timer;

        CUDA_CHECK(cudaMalloc(&d_a, bytes));
        CUDA_CHECK(cudaMalloc(&d_b, bytes));
        CUDA_CHECK(cudaMemcpy(d_a, host_a.data(), bytes, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemset(d_b, 0, bytes));

        const dim3 block(static_cast<unsigned int>(args.block_x),
                         static_cast<unsigned int>(args.block_y));
        const dim3 grid(static_cast<unsigned int>((ny - 2 + args.block_x - 1) / args.block_x),
                        static_cast<unsigned int>((nx - 2 + args.block_y - 1) / args.block_y));

        cudaEvent_t start_event{};
        cudaEvent_t stop_event{};
        CUDA_CHECK(cudaEventCreate(&start_event));
        CUDA_CHECK(cudaEventCreate(&stop_event));

        CUDA_CHECK(cudaEventRecord(start_event));
        for (int t = 0; t < steps; ++t) {
            // STARTER TODO:
            // Launch the kernel here, check for launch errors, and swap d_a and d_b.
            // Example shape only:
            //   stencil_step_kernel<<<grid, block>>>(d_a, d_b, nx, ny);
            //   CUDA_CHECK(cudaGetLastError());
            //   std::swap(d_a, d_b);
        }
        CUDA_CHECK(cudaEventRecord(stop_event));
        CUDA_CHECK(cudaEventSynchronize(stop_event));

        float compute_milliseconds = 0.0f;
        CUDA_CHECK(cudaEventElapsedTime(&compute_milliseconds, start_event, stop_event));

        CUDA_CHECK(cudaMemcpy(host_a.data(), d_a, bytes, cudaMemcpyDeviceToHost));
        const double end_to_end_seconds = end_to_end_timer.elapsed_seconds();

        CUDA_CHECK(cudaEventDestroy(start_event));
        CUDA_CHECK(cudaEventDestroy(stop_event));
        CUDA_CHECK(cudaFree(d_a));
        CUDA_CHECK(cudaFree(d_b));

        stencil::print_common_header(nx, ny, steps);
        std::cout << "block_x=" << args.block_x << "\n";
        std::cout << "block_y=" << args.block_y << "\n";
        std::cout << "compute_seconds=" << (compute_milliseconds / 1000.0f) << "\n";
        std::cout << "end_to_end_seconds=" << end_to_end_seconds << "\n";
        std::cout << std::setprecision(10);
        std::cout << "checksum=" << stencil::checksum(host_a.data(), n) << "\n";
#endif
#else
        std::vector<stencil::real_t> storage_a(n, 0.0f);
        std::vector<stencil::real_t> storage_b(n, 0.0f);
        stencil::real_t* a = storage_a.data();
        stencil::real_t* b = storage_b.data();
        stencil::initialize_hotspot(a, nx, ny);

#if defined(STENCIL_USE_OPENMP)
#ifdef _OPENMP
        omp_set_num_threads(args.threads);
#else
        std::cerr << "Warning: _OPENMP is not defined, so this build will run serially.\n";
#endif
#endif

#if defined(STENCIL_USE_OPENACC)
        const stencil::Timer end_to_end_timer;
        double compute_seconds = 0.0;

        {
            const stencil::Timer compute_timer;
            for (int t = 0; t < steps; ++t) {
                const stencil::real_t* src = (t % 2 == 0) ? a : b;
                stencil::real_t* dst = (t % 2 == 0) ? b : a;
                stencil::stencil_step(src, dst, nx, ny);
            }
            compute_seconds = compute_timer.elapsed_seconds();
        }

        const stencil::real_t* result = (steps % 2 == 0) ? a : b;
        const double end_to_end_seconds = end_to_end_timer.elapsed_seconds();

        stencil::print_common_header(nx, ny, steps);
        std::cout << "compute_seconds=" << compute_seconds << "\n";
        std::cout << "end_to_end_seconds=" << end_to_end_seconds << "\n";
        std::cout << std::setprecision(10);
        std::cout << "checksum=" << stencil::checksum(result, n) << "\n";
#else
        const stencil::Timer timer;
        for (int t = 0; t < steps; ++t) {
            const stencil::real_t* src = (t % 2 == 0) ? a : b;
            stencil::real_t* dst = (t % 2 == 0) ? b : a;
            stencil::stencil_step(src, dst, nx, ny);
        }
        const double runtime_seconds = timer.elapsed_seconds();
        const stencil::real_t* result = (steps % 2 == 0) ? a : b;

        stencil::print_common_header(nx, ny, steps);
#if defined(STENCIL_USE_OPENMP)
        std::cout << "threads=" << args.threads << "\n";
#endif
        std::cout << "runtime_seconds=" << runtime_seconds << "\n";
        std::cout << std::setprecision(10);
        std::cout << "checksum=" << stencil::checksum(result, n) << "\n";
#endif
#endif
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
