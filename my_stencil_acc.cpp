#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <openacc.h>

using real_t = float;

/*
salloc --partition=rss-class --gres=gpu:1 --cpus-per-task=16
  srun ./run_benchmarks.sh
*/

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

    std::vector<real_t> a(x * y, 0.0f);
    std::vector<real_t> b(x * y, 0.0f);

    real_t* p0 = a.data();
    real_t* p1 = b.data();

    const int cx = x / 2;
    const int cy = y / 2;
    const int hx = std::max(1, x / 8);
    const int hy = std::max(1, y / 8);

    for (int i = 1; i < x - 1; ++i) {
        for (int j = 1; j < y - 1; ++j) {
            if (std::abs(i - cx) < hx && std::abs(j - cy) < hy) {
                p0[i * y + j] = 100.0f;
            }
        }
    }

    std::string program_name = "my_stencil_acc";
    std::cout << "program=" << program_name << "\n";
    std::cout << "nx=" << x << "\n";
    std::cout << "ny=" << y << "\n";
    std::cout << "steps=" << iterations << "\n";

    Timer timer;

    #pragma acc data copy(p0[0:x*y], p1[0:x*y])
    {
        for (int i = 0; i < iterations; i += 2) {
            #pragma acc parallel loop collapse(2)
            for(int j = 1; j < x - 1; ++j)
                for(int k = 1; k < y - 1; ++k)
                    p1[j*y+k] = 0.2f*(p0[j*y+k]+p0[(j-1)*y+k]+p0[(j+1)*y+k]+p0[j*y+k-1]+p0[j*y+k+1]);

            if (i + 1 < iterations) {
                #pragma acc parallel loop collapse(2)
                for(int j = 1; j < x - 1; ++j)
                    for(int k = 1; k < y - 1; ++k)
                        p0[j*y+k] = 0.2f*(p1[j*y+k]+p1[(j-1)*y+k]+p1[(j+1)*y+k]+p1[j*y+k-1]+p1[j*y+k+1]);
            }
        }
    }
    double elapsed_seconds = timer.elapsed_seconds();
    std::cout << "elapsed_seconds=" << elapsed_seconds << "\n";
    real_t* src = (iterations & 1) ? p1 : p0;
    double checksum = 0.0;
    for (int i = 0; i < x * y; ++i) {
        checksum += src[i];
    }
    // Output the checksum in full precision to avoid truncation
    std::cout << "checksum=" << std::fixed << std::setprecision(6) << checksum << "\n";
    return 0;
}