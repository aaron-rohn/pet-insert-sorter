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

#define BASE_PORT 10000
namespace Sorter
{
    class socketbuf;
    ssize_t recvall(int, char*, size_t*);
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
    static const size_t max_size = 128; // max number of buffers to queue
    static const size_t buf_size = 8*1024*1024;
    static const size_t nsingles_per_buf = buf_size / Record::event_size;

    int fd;
    std::vector<char> current_buf;
    std::queue<std::vector<char>> recv_data;
    std::thread receiver;
    std::mutex lck;
    std::condition_variable cv;
    bool finished = false;
    unsigned long long nsingles = 0;

    void receive();
    int underflow();
    
    public:

    socketbuf(int fd):
        fd(fd), receiver(&socketbuf::receive, this) {}

    ~socketbuf() { receiver.join(); }
};

#endif
