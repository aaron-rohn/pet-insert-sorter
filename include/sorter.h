#ifndef SORTER_H
#define SORTER_H

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <chrono>
#include <vector>
#include <tuple>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <future>
#include <bitset>
#include <unistd.h>

#include "singles.h"
#include "coincidence.h"

#define BASE_PORT 10000
namespace Sorter
{
    Coincidences sort_span(const std::vector<cspan<SingleData>>);

    void read_socket(
            int, std::mutex&,
            std::atomic_bool&,
            std::condition_variable&,
            std::queue<cspan<SingleData>>&);

    void sort_data(
            std::string, std::mutex&,
            std::vector<std::atomic_bool>&,
            std::vector<std::condition_variable>&,
            std::vector<std::queue<cspan<SingleData>>>&);
}

#endif
