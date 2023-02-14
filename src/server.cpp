#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <tuple>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <future>

#include "../include/singles.h"
#include "../include/coincidence.h"

#define BASE_PORT 10000

std::pair<std::vector<int>, std::vector<int>> init_connections(int n)
{
    std::vector<int> sfd, cfd;

    for (int i = 0; i < n; i++)
    {
        int opt = 1;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

        int result;
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(BASE_PORT + i);

        result = bind(fd, (struct sockaddr*)&address, sizeof(address));
        result = listen(fd, 0);
        sfd.push_back(fd);
    }

    for (auto fd : sfd)
    {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int new_fd = accept(fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        cfd.push_back(new_fd);
    }

    return std::make_pair(sfd, cfd);
}

void close_connections(std::vector<int> &sfd, std::vector<int> &cfd)
{
    for (int fd : cfd)
        close(fd);

    for (int fd : sfd)
        shutdown(fd, SHUT_RDWR);
}

void read_socket(
        int fd, std::mutex &lck,
        std::atomic_bool &finished,
        std::condition_variable &data_cv,
        std::queue<std::vector<Single>> &event_queue
) {
    char stream_buf[1024*1024*6];
    socketbuf sb(fd, stream_buf, sizeof(stream_buf));
    std::istream singles_stream(&sb);

    Record::go_to_tt(singles_stream, 0);

    {
        std::unique_lock<std::mutex> ulck(lck);
        std::cout << "got reset after receiving " << sb.total_read << " bytes" << std::endl;
    }

    size_t tt_per_sec = 10000;
    unsigned long long current_tt = tt_per_sec;
    while (singles_stream.good())
    {
        auto singles = Record::go_to_tt(singles_stream, current_tt);
        current_tt += tt_per_sec;

        std::unique_lock<std::mutex> ulck(lck);
        event_queue.push(std::move(singles));
        data_cv.notify_all();
    }

    std::unique_lock<std::mutex> ulck(lck);
    finished = true;
    data_cv.notify_all();
}

void sort_data(
        std::string coincidence_file, std::mutex &lck,
        std::vector<std::atomic_bool> &finished,
        std::vector<std::condition_variable> &singles_cv,
        std::vector<std::queue<std::vector<Single>>> &singles_queues
) {
    std::ofstream cf(coincidence_file, std::ios::out | std::ios::binary);
    const size_t n = singles_cv.size();

    std::queue<std::future<Coincidences>> sorters;

    while (true)
    {
        bool all_finished = std::all_of(finished.begin(), finished.end(),
                [&](std::atomic_bool &f){ return bool(f); });
        if (all_finished && sorters.size() == 0) break;

        if (!all_finished)
        {
            std::vector<std::vector<Single>> singles(n);

            for (int i = 0; i < n; i++)
            {
                auto &sq = singles_queues[i];

                // wait for each queue to have data available
                std::unique_lock<std::mutex> ulck(lck);
                singles_cv[i].wait(ulck,
                        [&]{ return finished[i] || !sq.empty(); });

                if (sq.size() > 0)
                {
                    singles[i] = std::move(sq.front());
                    sq.pop();
                }
            }

            sorters.push(std::async(std::launch::async,
                        &CoincidenceData::sort_span, std::move(singles)));
        }

        auto status = sorters.front().wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready)
        {
            Coincidences cd = sorters.front().get();
            CoincidenceData::write(cf, cd);
            sorters.pop();
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: sorter coincidence_filename" << std::endl;
        exit(1);
    }
    std::string coin_filename(argv[1]);

    auto [sfd, cfd] = init_connections(4);
    const size_t n = cfd.size();

    std::vector<std::atomic_bool> finished(n);
    for (auto &f : finished) f = false;

    std::mutex lck;
    std::vector<std::condition_variable> data_cv(n);
    std::vector<std::queue<std::vector<Single>>> data_queues(n);

    std::vector<std::thread> readers;
    for (int i = 0; i < n; i++)
    {
        readers.emplace_back(read_socket, cfd[i],
                std::ref(lck),
                std::ref(finished[i]),
                std::ref(data_cv[i]),
                std::ref(data_queues[i]));
    }

    std::thread coincidence_sorter (sort_data,
            coin_filename,
            std::ref(lck),
            std::ref(finished),
            std::ref(data_cv),
            std::ref(data_queues));

    for (auto &t : readers)
        t.join();

    coincidence_sorter.join();

    close_connections(sfd, cfd);
    return 0;
}