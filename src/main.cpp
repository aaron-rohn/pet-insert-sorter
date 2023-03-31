#include <netinet/in.h>
#include "sorter.h"

std::pair<std::vector<int>, std::vector<int>>
init_connections(int n)
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

    std::cout << "Listening on 127.0.0.1 port "  << BASE_PORT << ":" << BASE_PORT+n-1 << std::endl;

    for (auto fd : sfd)
    {
        struct sockaddr_in address;
        int addrlen = sizeof(address);
        int new_fd = accept(fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        cfd.push_back(new_fd);
    }

    return std::make_pair(sfd, cfd);
}

void close_connections(
        std::vector<int> &sfd,
        std::vector<int> &cfd
) {
    for (int fd : cfd)
        close(fd);

    for (int fd : sfd)
        shutdown(fd, SHUT_RDWR);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: sorter coincidence_filename" << std::endl;
        exit(1);
    }
    std::string coin_filename(argv[1]);

    const size_t n = 4;
    auto [sfd, cfd] = init_connections(n);

    std::vector<std::atomic_bool> finished(n);
    for (auto &f : finished) f = false;

    std::mutex lck;
    std::vector<std::condition_variable> data_cv(n);
    std::vector<std::queue<cspan<SingleData>>> data_queues(n);

    std::vector<std::thread> readers;
    for (int i = 0; i < n; i++)
    {
        readers.emplace_back(Sorter::read_socket,
                cfd[i],
                std::ref(lck),
                std::ref(finished[i]),
                std::ref(data_cv[i]),
                std::ref(data_queues[i]));
    }

    std::thread coincidence_sorter (Sorter::sort_data,
            coin_filename,
            std::ref(lck),
            std::ref(finished),
            std::ref(data_cv),
            std::ref(data_queues));

    for (auto &t : readers) t.join();
    coincidence_sorter.join();
    close_connections(sfd, cfd);
    return 0;
}
