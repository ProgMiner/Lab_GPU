#include <libutils/misc.h>
#include <libutils/timer.h>
#include <libutils/fast_random.h>
#include <libgpu/context.h>
#include <libgpu/shared_device_buffer.h>

#include "cl/sum_cl.h"


template<typename T>
void raiseFail(const T &a, const T &b, std::string message, std::string filename, int line)
{
    if (a != b) {
        std::cerr << message << " But " << a << " != " << b << ", " << filename << ":" << line << std::endl;
        throw std::runtime_error(message);
    }
}

#define EXPECT_THE_SAME(a, b, message) raiseFail(a, b, message, __FILE__, __LINE__)


int main(int argc, char **argv) {
    const int benchmarkingIters = 10;

    const unsigned int n = 100*1000*1000;
    std::vector<unsigned int> as(n, 0);
    unsigned int reference_sum = 0;
    FastRandom r(42);

    for (int i = 0; i < n; ++i) {
        as[i] = (unsigned int) r.next(0, std::numeric_limits<unsigned int>::max() / n);
        reference_sum += as[i];
    }

    {
        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int sum = 0;
            for (int i = 0; i < n; ++i) {
                sum += as[i];
            }
            EXPECT_THE_SAME(reference_sum, sum, "CPU result should be consistent!");
            t.nextLap();
        }
        std::cout << "CPU:     " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "CPU:     " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    {
        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int sum = 0;
            #pragma omp parallel for reduction(+:sum)
            for (int i = 0; i < n; ++i) {
                sum += as[i];
            }
            EXPECT_THE_SAME(reference_sum, sum, "CPU OpenMP result should be consistent!");
            t.nextLap();
        }
        std::cout << "CPU OMP: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "CPU OMP: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    const gpu::Device device = gpu::chooseGPUDevice(argc, argv);

    gpu::Context context;
    context.init(device.device_id_opencl);
    context.activate();

    gpu::gpu_mem_32u as_gpu;

    as_gpu.resizeN(n);
    as_gpu.writeN(as.data(), n);

    gpu::gpu_mem_32u result_gpu;
    result_gpu.resizeN(1);

    {
        ocl::Kernel kernel(sum_kernel, sum_kernel_length, "sum_naive");
        kernel.compile(true);

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int result = 0;

            result_gpu.writeN(&result, 1);

            const unsigned int workGroupSize = 128;
            const unsigned int global_work_size = (n + workGroupSize - 1)
                / workGroupSize * workGroupSize;
            kernel.exec(gpu::WorkSize(workGroupSize, global_work_size), as_gpu, n, result_gpu);

            result_gpu.readN(&result, 1);
            EXPECT_THE_SAME(reference_sum, result, "GPU result must be equal to CPU result!");

            t.nextLap();
        }

        std::cout << "GPU naïve: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU naïve: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    {
        ocl::Kernel kernel(sum_kernel, sum_kernel_length, "sum_loop");
        kernel.compile(true);

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int result = 0;

            result_gpu.writeN(&result, 1);

            const unsigned int workGroupSize = 128;
            const unsigned int global_work_size = (n / 64 + workGroupSize - 1)
                / workGroupSize * workGroupSize;
            kernel.exec(gpu::WorkSize(workGroupSize, global_work_size), as_gpu, n, result_gpu);

            result_gpu.readN(&result, 1);
            EXPECT_THE_SAME(reference_sum, result, "GPU result must be equal to CPU result!");

            t.nextLap();
        }

        std::cout << "GPU loop: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU loop: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    {
        ocl::Kernel kernel(sum_kernel, sum_kernel_length, "sum_loop_coalesced");
        kernel.compile(true);

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int result = 0;

            result_gpu.writeN(&result, 1);

            const unsigned int workGroupSize = 128;
            const unsigned int global_work_size = (n / 64 + workGroupSize - 1)
                / workGroupSize * workGroupSize;
            kernel.exec(gpu::WorkSize(workGroupSize, global_work_size), as_gpu, n, result_gpu);

            result_gpu.readN(&result, 1);
            EXPECT_THE_SAME(reference_sum, result, "GPU result must be equal to CPU result!");

            t.nextLap();
        }

        std::cout << "GPU loop coalesced: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU loop coalesced: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    {
        ocl::Kernel kernel(sum_kernel, sum_kernel_length, "sum_local");
        kernel.compile(true);

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int result = 0;

            result_gpu.writeN(&result, 1);

            const unsigned int workGroupSize = 128;
            const unsigned int global_work_size = (n + workGroupSize - 1)
                / workGroupSize * workGroupSize;
            kernel.exec(gpu::WorkSize(workGroupSize, global_work_size), as_gpu, n, result_gpu);

            result_gpu.readN(&result, 1);
            EXPECT_THE_SAME(reference_sum, result, "GPU result must be equal to CPU result!");

            t.nextLap();
        }

        std::cout << "GPU local: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU local: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }

    {
        ocl::Kernel kernel(sum_kernel, sum_kernel_length, "sum_tree");
        kernel.compile(true);

        timer t;
        for (int iter = 0; iter < benchmarkingIters; ++iter) {
            unsigned int result = 0;

            result_gpu.writeN(&result, 1);

            const unsigned int workGroupSize = 128;
            const unsigned int global_work_size = (n + workGroupSize - 1)
                / workGroupSize * workGroupSize;
            kernel.exec(gpu::WorkSize(workGroupSize, global_work_size), as_gpu, n, result_gpu);

            result_gpu.readN(&result, 1);
            EXPECT_THE_SAME(reference_sum, result, "GPU result must be equal to CPU result!");

            t.nextLap();
        }

        std::cout << "GPU tree: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "GPU tree: " << (n/1000.0/1000.0) / t.lapAvg() << " millions/s" << std::endl;
    }
}