/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

template<typename T>
void read_gnn_batch(const std::string & file_name, std::vector<T> & indices) {
    int fd = open(file_name.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error opening batch file: " << file_name << std::endl;
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "Error getting file size: " << file_name << std::endl;
        return;
    }

    char * file_data = static_cast<char *>(mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (file_data == MAP_FAILED) {
        std::cerr << "Error mapping file to memory: " << file_name << std::endl;
        return;
    }

    std::string index_str;

    // Start timer
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < sb.st_size; i++) {
        if (file_data[i] == ',') {
            indices.push_back(std::stoi(index_str));
            index_str.clear();
        } else {
            index_str += file_data[i];
        }
    }
    if (!index_str.empty()) {
        indices.push_back(std::stoi(index_str));
    }
    // End timer
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //std::cout << "Time taken to read file: " << duration.count() << " microseconds" << std::endl;

    if (munmap(file_data, sb.st_size) == -1) {
        std::cerr << "Error unmapping memory" << file_name << std::endl;
        return;
    }

    close(fd);
}