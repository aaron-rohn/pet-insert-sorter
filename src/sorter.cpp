#include "sorter.h"

std::vector<Single> Sorter::go_to_tt(
        std::istream &f,
        uint64_t value,
        uint64_t approx_size
) {
    uint8_t data[Record::event_size];
    uint64_t last_tt_value = 0;
    bool synced = false;
    TimeTag tt;

    std::vector<Single> sgls;
    if (approx_size > 0)
        sgls.reserve(approx_size);

    while(f.good())
    {
        Record::read(f, data);
        Record::align(f, data);

        if (!Record::is_single(data))
        {
            tt = TimeTag(data);
            synced = (tt.value == (last_tt_value+1));
            last_tt_value = tt.value;

            if ((value == 0 && tt.value == 0) ||
                (synced && value > 0 && tt.value >= value))
            {
                break;
            }
        }
        else
        {
            // don't save events when waiting for reset
            if (value != 0)
                sgls.emplace_back(data, tt);
        }
    }

    return sgls;
}

ssize_t Sorter::recvall(int fd, char *ptr, size_t sz)
{
    char *beg = ptr, *end = beg + sz;
    while (beg < end)
    {
        ssize_t recv = read(fd, beg, end - beg);
        beg += recv;

        // check if error or socket closed
        if (recv < 1) return recv;
    }
    return sz;
}

void Sorter::socketbuf::receive()
{
    //auto start = std::chrono::high_resolution_clock::now();

    while (!finished)
    {
        std::vector<char> buf(buf_size);
        size_t n = 0;
        ssize_t result = Sorter::recvall(fd, buf.data(), buf_size);

        {
            std::lock_guard<std::mutex> lg(lck);

            // check if error or socket closed
            if (result < 1) finished = true;

            // push the data to the queue
            else if (recv_data.size() < max_size)
            {
                recv_data.push(std::move(buf));
                nsingles += nsingles_per_buf;
            }
            // else overflow
        }

        cv.notify_all();
    }

    /*
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    auto ds = std::chrono::duration_cast<std::chrono::seconds>(duration);

    std::lock_guard<std::mutex> lg(lck);
    std::cout << nsingles << " " << ds.count() << std::endl;
    */
}

int Sorter::socketbuf::underflow()
{
    {
        // try to pull a new data from the queue
        std::unique_lock<std::mutex> lg(lck);
        cv.wait(lg, [&]{ return finished || !recv_data.empty(); });

        // check if error or socket closed
        if (finished && recv_data.empty()) return traits_type::eof();

        current_buf = std::move(recv_data.front());
        recv_data.pop();
    }

    // update streambuf pointers
    char *ptr = current_buf.data();
    setg(ptr, ptr, ptr + current_buf.size());
    return traits_type::to_int_type(*gptr());
}

Coincidences Sorter::sort_span(
        std::vector<std::vector<Single>> all_singles
) {
    size_t n = 0, offset = 0;
    for (const auto &sgl : all_singles)
        n += sgl.size();

    std::vector<Single> singles(n);
    for (const auto &sgl : all_singles)
    {
        auto size = sgl.size() * sizeof(Single);
        std::memcpy((char*)singles.data() + offset,
                    (char*)sgl.data(), size);
        offset += size;
    }

    std::sort(singles.begin(), singles.end());
    return CoincidenceData::sort(singles);
}

void Sorter::read_socket(
        int fd, std::mutex &lck,
        std::atomic_bool &finished,
        std::condition_variable &data_cv,
        std::queue<std::vector<Single>> &event_queue
) {
    Sorter::socketbuf sb(fd);
    std::istream singles_stream(&sb);

    Sorter::go_to_tt(singles_stream, 0, 0);

    {
        std::lock_guard<std::mutex> lg(lck);
        std::cout << "Found reset" << std::endl;
    }

    uint64_t tt_incr = 1000, current_tt = tt_incr, nsingles = 0;

    while (singles_stream.good())
    {
        auto singles = Sorter::go_to_tt(singles_stream, current_tt, nsingles);
        nsingles = singles.size() * 1.1; // overestimate when reserving space
        current_tt += tt_incr;

        std::unique_lock<std::mutex> ulck(lck);
        event_queue.push(std::move(singles));
        data_cv.notify_all();
    }

    std::unique_lock<std::mutex> ulck(lck);
    finished = true;
    data_cv.notify_all();
}

void Sorter::sort_data(
        std::string coincidence_file,
        std::mutex &lck,
        std::vector<std::atomic_bool> &finished,
        std::vector<std::condition_variable> &singles_cv,
        std::vector<std::queue<std::vector<Single>>> &singles_queues
) {
    std::ofstream cf(coincidence_file, std::ios::out | std::ios::binary);
    const size_t n = singles_queues.size();
    std::queue<std::future<Coincidences>> sorters;
    bool all_finished = false;

    while (!all_finished || !sorters.empty())
    {
        if (!all_finished)
        {
            std::vector<std::vector<Single>> singles(n);
            all_finished = true;

            for (int i = 0; i < n; i++)
            {
                auto &sq = singles_queues[i];

                // wait for each queue to have data available
                std::unique_lock<std::mutex> ulck(lck);
                singles_cv[i].wait(ulck,
                        [&]{ return finished[i] || !sq.empty(); });

                all_finished &= finished[i];
                if (!sq.empty())
                {
                    singles[i] = std::move(sq.front());
                    sq.pop();
                }
            }

            sorters.push(std::async(std::launch::async,
                        &Sorter::sort_span, std::move(singles)));
        }

        if (!sorters.empty())
        {
            // check if the oldest worker has finished sorting
            auto status = sorters.front().wait_for(std::chrono::seconds(0));
            if (status == std::future_status::ready)
            {
                // get and save the sorted coincidences
                Coincidences cd = sorters.front().get();
                CoincidenceData::write(cf, cd);
                sorters.pop();
            }
        }
    }
}
