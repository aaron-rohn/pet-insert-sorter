#include "sorter.h"
#include <numeric>

Coincidences Sorter::sort_span(
        const std::vector<cspan<SingleData>> all_singles 
) {
    size_t n = 0;
    for (const auto &sgl: all_singles)
        n += sgl.size;

    std::vector<SingleData> singles;
    singles.reserve(n);

    for (const auto &sgl : all_singles)
        singles.insert(singles.end(), sgl.begin(), sgl.end());

    std::ranges::sort(singles, {}, &SingleData::abstime);
    return CoincidenceData::sort(singles);
}

void Sorter::read_socket(
        int fd, std::mutex &lck,
        std::atomic_bool &finished,
        std::condition_variable &data_cv,
        std::queue<cspan<SingleData>> &event_queue
) {
    SinglesReader rdr = reader_new(fd, 0);
    go_to_tt(&rdr, 0);

    {
        std::lock_guard<std::mutex> lg(lck);
        std::cout << "Found reset" << std::endl;
    }

    uint64_t tt_incr = 1000, current_tt = tt_incr, nsingles = 0;

    while(!rdr.finished)
    {
        auto singles = span_singles_to_tt(&rdr, current_tt, &nsingles);

        {
            std::unique_lock<std::mutex> ulck(lck);
            event_queue.emplace(std::move(singles));
        }

        data_cv.notify_all();

        nsingles = nsingles * 1.1; // slightly over-estimate when allocating space
        current_tt += tt_incr;
    }

    {
        std::unique_lock<std::mutex> ulck(lck);
        finished = true;
    }

    data_cv.notify_all();
}

void Sorter::sort_data(
        std::string coincidence_file,
        std::mutex &lck,
        std::vector<std::atomic_bool> &finished,
        std::vector<std::condition_variable> &singles_cv,
        std::vector<std::queue<cspan<SingleData>>> &singles_queues
) {
    std::ofstream cf(coincidence_file, std::ios::out | std::ios::binary);
    const size_t n = singles_queues.size();
    std::queue<std::future<Coincidences>> sorters;
    bool all_finished = false;

    while (!all_finished || !sorters.empty())
    {
        if (!all_finished)
        {
            std::vector<cspan<SingleData>> singles(n);
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
