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
#include <future>
#include <bitset>
#include <unistd.h>

#include "singles.h"
#include "coincidence.h"

#define BASE_PORT 20000
#define SIZE (1024*1024*8)

namespace Sorter
{
    class socketbuf;
    ssize_t recvall(int fd, char *ptr, size_t sz);
    std::vector<Single> go_to_tt(std::istream& ,uint64_t, uint64_t);
    Coincidences sort_span(std::vector<std::vector<Single>>);

    void read_socket(
            int,
            std::mutex&,
            std::atomic_bool&,
            std::condition_variable&,
            std::queue<std::vector<Single>>&);

    void sort_data(
            std::string,
            std::mutex&,
            std::vector<std::atomic_bool>&,
            std::vector<std::condition_variable>&,
            std::vector<std::queue<std::vector<Single>>>&);
}

class Sorter::socketbuf: public std::streambuf
{
    typedef std::streambuf::traits_type traits_type;

    int fd;
    std::vector<char> current_buf;
    std::queue<std::vector<char>> recv_data;
    std::thread receiver;
    std::mutex lck;
    std::condition_variable cv;
    bool finished = false;

    void receive();
    int underflow();
    
    public:

    socketbuf(int fd):
        fd(fd), receiver(&socketbuf::receive, this) {}

    ~socketbuf() { receiver.join(); }
};

#endif
