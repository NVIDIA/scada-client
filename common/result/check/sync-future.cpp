/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <psa/futures>

#include <iostream>
#include <list>
#include <tuple>

using result = psa::futures::result<std::tuple<>, int>;

struct scheduler {
    std::list<std::coroutine_handle<>> pending_;

    struct pending {
        scheduler & scheduler_;

        inline pending(scheduler & sched) noexcept : scheduler_(sched) {}
        pending(pending &&) noexcept = default;

        bool is_ready() const noexcept { return false; }
        std::function<void(void)> schedule(std::coroutine_handle<> h) const noexcept {
            return [h, this](void) { scheduler_.pending_.emplace_back(h); };
        }
        result::sync_type value() const noexcept { return result::sync_type::ok<>(); }
    };

    result async() {
        std::cerr << "Before Scheduler Suspend" << std::endl;
        co_await pending(*this);
        std::cerr << "After Scheduler Suspend" << std::endl;
        co_return result::sync_type::ok(std::tuple<>());
    }

    void run() {
        while (pending_.empty() == false) {
            std::cerr << "Calling pending async" << std::endl;
            pending_.front().resume();
            pending_.pop_front();
        }
    }
};

result mock_io(scheduler & sched) {
    std::cerr << "Calling Scheduler" << std::endl;
    auto ret = co_await sched.async();
    std::cerr << "Return from Scheduler" << std::endl;
    co_return ret;
}

int main(int argc, char * argv[]) {
    scheduler sched;

    std::cerr << "Call Async I/O" << std::endl;
    auto ret = mock_io(sched);
    std::cerr << "Return from Async I/O" << std::endl;

    sched.run();

    return EXIT_SUCCESS;
}
