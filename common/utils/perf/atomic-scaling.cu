/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gdakis/atomic>
#include <iomanip>
#include <iostream>
#include <list>
#include <psa/result>
#include <tuple>

#include <sys/ioctl.h>
#include <unistd.h>

template<gdakis::memory_order>
struct memory_order;

template<>
struct memory_order<gdakis::memory_order_relaxed> {
    static constexpr const char * name = "relaxed";
};
template<>
struct memory_order<gdakis::memory_order_acquire> {
    static constexpr const char * name = "acquire";
};
template<>
struct memory_order<gdakis::memory_order_release> {
    static constexpr const char * name = "release";
};
template<>
struct memory_order<gdakis::memory_order_acq_rel> {
    static constexpr const char * name = "acquire-release";
};

static inline const char * memory_order_name(gdakis::memory_order mem_order) {
    switch (mem_order) {
    case gdakis::memory_order_relaxed:
        return memory_order<gdakis::memory_order_relaxed>::name;
    case gdakis::memory_order_acquire:
        return memory_order<gdakis::memory_order_acquire>::name;
    case gdakis::memory_order_release:
        return memory_order<gdakis::memory_order_release>::name;
    case gdakis::memory_order_acq_rel:
        return memory_order<gdakis::memory_order_acq_rel>::name;
    default:
        return "unknown";
    }
}

template<gdakis::thread_scope _Sco>
struct thread_scope;
template<>
struct thread_scope<gdakis::thread_scope_thread> {
    static constexpr const char * name = "thread";
};
template<>
struct thread_scope<gdakis::thread_scope_block> {
    static constexpr const char * name = "block";
};
template<>
struct thread_scope<gdakis::thread_scope_device> {
    static constexpr const char * name = "device";
};
template<>
struct thread_scope<gdakis::thread_scope_system> {
    static constexpr const char * name = "system";
};
static inline const char * thread_scope_name(gdakis::thread_scope scope) {
    switch (scope) {
    case gdakis::thread_scope_thread:
        return thread_scope<gdakis::thread_scope_thread>::name;
    case gdakis::thread_scope_block:
        return thread_scope<gdakis::thread_scope_block>::name;
    case gdakis::thread_scope_device:
        return thread_scope<gdakis::thread_scope_device>::name;
    case gdakis::thread_scope_system:
        return thread_scope<gdakis::thread_scope_system>::name;
    }
    return "unknown";
}

using record_type = std::tuple<std::string, gdakis::memory_order, gdakis::thread_scope, unsigned, float>;

template<typename _Fn, typename... Args>
psa::result<float, cudaError_t> time_kernel_run(const dim3 & dg, const dim3 & db, _Fn && fn, Args &&... args) {
    using return_type = psa::result<float, cudaError_t>;
    cudaEvent_t start, stop;
    cudaError_t err;

    err = cudaEventCreate(&start);
    if (err != cudaSuccess)
        return return_type::err(err);
    err = cudaEventCreate(&stop);
    if (err != cudaSuccess)
        return return_type::err(err);

    err = cudaEventRecord(start);
    if (err != cudaSuccess)
        return return_type::err(err);
    fn<<<dg, db>>>(std::forward<Args>(args)...);
    err = cudaEventRecord(stop);
    if (err != cudaSuccess)
        return return_type::err(err);

    err = cudaEventSynchronize(stop);
    if (err != cudaSuccess)
        return return_type::err(err);

    float value;
    err = cudaEventElapsedTime(&value, start, stop);
    if (err != cudaSuccess)
        return return_type::err(err);

    cudaEventDestroy(stop);
    cudaEventDestroy(start);

    return return_type::ok(value);
}

template<gdakis::memory_order _Order, gdakis::thread_scope _Sco>
static __global__ void atomic_red_kernel(gdakis::atomic<unsigned, _Sco> * atomic, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    atomic->fetch_add(1, _Order);
    out[id] = id;
}

template<gdakis::memory_order _Order, gdakis::thread_scope _Sco>
static __global__ void atomic_atom_kernel(gdakis::atomic<unsigned, _Sco> * atomic, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    out[id] = atomic->fetch_add(1, _Order);
}

template<gdakis::memory_order _Order, gdakis::thread_scope _Sco>
static __global__ void atomic_cas_kernel(gdakis::atomic<unsigned, _Sco> * atomic, unsigned * out) {
    unsigned id = threadIdx.x + blockIdx.x * blockDim.x;
    unsigned old = atomic->load(_Order);
    while (atomic->compare_exchange_strong(old, old + 1, _Order, _Order) == false)
        ;
    out[id] = old + 1;
}

static inline void dump_results(const char * name, const std::list<record_type> & results) {
    for (const auto & row : results) {
        std::cout << name << ", " << std::get<0>(row) << ", " << memory_order_name(std::get<1>(row)) << ", "
                  << thread_scope_name(std::get<2>(row)) << ", " << std::get<3>(row) << ", " << std::get<4>(row)
                  << std::endl;
    }
}

static inline void print_progress(double total, double current) {
    struct winsize size;
    ioctl(STDERR_FILENO, TIOCGWINSZ, &size);

    double progress = current / total;
    unsigned marks = (size.ws_col - 12) * progress;
    std::cerr << "  [";
    unsigned n;
    for (n = 0; n < marks; ++n)
        std::cerr << "#";
    for (; n < size.ws_col - 12; ++n)
        std::cerr << " ";
    std::cerr << "] " << std::fixed << std::setw(6) << std::setprecision(2) << 100.0 * progress << "%\r" << std::flush;
}

template<gdakis::memory_order _Order, gdakis::thread_scope _Sco>
psa::result<std::list<record_type>, cudaError_t> run_experiments() {
    std::cerr << "  Running experiments for scope '" << thread_scope<_Sco>::name << "'" << std::endl;

    std::list<record_type> results;
    using return_type = psa::result<std::list<record_type>, cudaError_t>;

    gdakis::atomic<unsigned, _Sco> * atomic;
    auto cuda_ret = cudaMalloc(&atomic, sizeof(*atomic));
    if (cuda_ret != cudaSuccess) {
        std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(cuda_ret) << std::endl;
        return return_type::err(cuda_ret);
    }

    static const unsigned block_size = 128;
    static const unsigned max_order = 24;
    unsigned long max_threads = 1ULL << max_order;
    unsigned * out;
    cuda_ret = cudaMalloc(&out, max_threads * sizeof(unsigned));
    if (cuda_ret != cudaSuccess) {
        std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(cuda_ret) << std::endl;
        return return_type::err(cuda_ret);
    }

    double total = 3 * ((1ULL << max_order) - 1);
    double current = 0;
    print_progress(total, current);
    for (auto n = 0; n < max_order; ++n) {
        auto n_threads = 1ULL << n;

        cuda_ret = cudaMemset(atomic, 0, sizeof(*atomic));
        if (cuda_ret != cudaSuccess) {
            std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(cuda_ret) << std::endl;
            return return_type::err(cuda_ret);
        }

        dim3 db = n_threads < block_size ? n_threads : block_size;
        dim3 dg = (n_threads + db.x - 1) / db.x;

        auto ret = time_kernel_run(dg, db, atomic_red_kernel<_Order, _Sco>, atomic, out);
        if (ret.is_err()) {
            std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(ret.err()) << std::endl;
            return return_type::err(ret.err());
        }
        results.emplace_back(std::make_tuple("red", _Order, _Sco, n_threads, ret.ok()));
        current += n_threads;
        print_progress(total, current);

        ret = time_kernel_run(dg, db, atomic_atom_kernel<_Order, _Sco>, atomic, out);
        if (ret.is_err()) {
            std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(ret.err()) << std::endl;
            return return_type::err(ret.err());
        }
        results.emplace_back(std::make_tuple("atom", _Order, _Sco, n_threads, ret.ok()));
        current += n_threads;
        print_progress(total, current);

        ret = time_kernel_run(dg, db, atomic_cas_kernel<_Order, _Sco>, atomic, out);
        if (ret.is_err()) {
            std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(ret.err()) << std::endl;
            return return_type::err(ret.err());
        }
        results.emplace_back(std::make_tuple("cas", _Order, _Sco, n_threads, ret.ok()));
        current += n_threads;
        print_progress(total, current);
    }
    std::cerr << std::endl;

    cudaFree(out);
    cudaFree(atomic);
    return return_type::ok(std::move(results));
}

template<gdakis::memory_order _Order>
psa::result<std::list<record_type>, cudaError_t> run_experiment_set() {
    std::cerr << "Running Experiment Set for memory order '" << memory_order<_Order>::name << "'" << std::endl;

    using return_type = psa::result<std::list<record_type>, cudaError_t>;
    std::list<record_type> results;

    auto ret = run_experiments<_Order, gdakis::thread_scope_block>();
    if (ret.is_err())
        return return_type::err(ret.err());
    results.splice(results.end(), ret.take_ok());

    ret = run_experiments<_Order, gdakis::thread_scope_device>();
    if (ret.is_err())
        return return_type::err(ret.err());
    results.splice(results.end(), ret.take_ok());

    ret = run_experiments<_Order, gdakis::thread_scope_system>();
    if (ret.is_err())
        return return_type::err(ret.err());
    results.splice(results.end(), ret.take_ok());

    return return_type::ok(std::move(results));
}

int main(int argc, char * argv[]) {
    cudaError_t cuda_ret;

    cudaDeviceProp cuda_prop;
    cuda_ret = cudaGetDeviceProperties(&cuda_prop, 0);
    if (cuda_ret != cudaSuccess) {
        std::cerr << "ERROR[" << __LINE__ << "]: " << cudaGetErrorString(cuda_ret) << std::endl;
        return EXIT_SUCCESS;
    }

    std::list<record_type> results;

    auto ret = run_experiment_set<gdakis::memory_order_relaxed>();
    if (ret.is_err())
        return EXIT_FAILURE;
    results.splice(results.end(), ret.take_ok());

    ret = run_experiment_set<gdakis::memory_order_acquire>();
    if (ret.is_err())
        return EXIT_FAILURE;
    results.splice(results.end(), ret.take_ok());

    ret = run_experiment_set<gdakis::memory_order_release>();
    if (ret.is_err())
        return EXIT_FAILURE;
    results.splice(results.end(), ret.take_ok());

    ret = run_experiment_set<gdakis::memory_order_acq_rel>();
    if (ret.is_err())
        return EXIT_FAILURE;
    results.splice(results.end(), ret.take_ok());

    dump_results(cuda_prop.name, results);

    return EXIT_SUCCESS;
}