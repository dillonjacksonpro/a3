#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <omp.h>

using real_t = float;

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

int main(int argc, char* argv[]) {

    int x = argc > 1 ? std::stoi(argv[1]) : 1000;
    int y = argc > 2 ? std::stoi(argv[2]) : 1000;
    int iterations = argc > 3 ? std::stoi(argv[3]) : 100;
    int threads = argc > 4 ? std::stoi(argv[4]) : 1;

    std::vector<real_t> a(x * y, 0.0f);
    std::vector<real_t> b(x * y, 0.0f);

    real_t* src = a.data();
    real_t* dst = b.data();

    const int cx = x / 2;
    const int cy = y / 2;
    const int hx = std::max(1, x / 8);
    const int hy = std::max(1, y / 8);

    for (int i = 1; i < x - 1; ++i) {
        for (int j = 1; j < y - 1; ++j) {
            if (std::abs(i - cx) < hx && std::abs(j - cy) < hy) {
                src[i * y + j] = 100.0f;
            }
        }
    }

    std::string program_name = "my stencil_omp";
    std::cout << "program=" << program_name << "\n";
    std::cout << "nx=" << x << "\n";
    std::cout << "ny=" << y << "\n";
    std::cout << "steps=" << iterations << "\n";
    std::cout << "threads=" << threads << "\n";

    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        #pragma omp parallel for num_threads(threads)
        for(int j = 1; j < x - 1; ++j) {
            for(int k = 1; k < y - 1; ++k) {
                dst[j * y + k] = 0.2f * (src[j * y + k] + src[(j-1) * y + k] + src[(j+1) * y + k] + src[j * y + k-1] + src[j * y + k+1]);
            }
        }
        std::swap(src, dst);
    }
    double elapsed_seconds = timer.elapsed_seconds();
    std::cout << "elapsed_seconds=" << elapsed_seconds << "\n";
    double checksum = 0.0;
    for (int i = 0; i < x * y; ++i) {
        checksum += src[i];
    }
    // Output the checksum in full precision to avoid truncation
    std::cout << "checksum=" << std::fixed << std::setprecision(6) << checksum << "\n";
    return 0;
}