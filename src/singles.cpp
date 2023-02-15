
#include "../include/singles.h"

#include <iostream>
#include <vector>

void Record::align(std::istream &f, uint8_t data[])
{
    while (f.good() && !is_header(data[0]))
    {
        size_t n = event_size;
        for (size_t i = 1; i < event_size; i++)
        {
            if (is_header(data[i]))
            {
                std::memmove(data, data + i, event_size - i);
                n = i;
                break;
            }
        }
        read(f, data + event_size - n, n);
    }
}

std::vector<Single> Record::go_to_tt(
        std::istream &f,
        uint64_t value,
        uint64_t approx_size
) {
    uint8_t data[event_size];
    uint64_t last_tt_value = 0;
    bool synced = false;
    TimeTag tt;

    std::vector<Single> sgls;
    if (approx_size > 0)
        sgls.reserve(approx_size);

    while(f.good())
    {
        read(f, data);
        align(f, data);

        if (!is_single(data))
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

void socketbuf::receive()
{
    while (true)
    {
        std::array<char, SIZE> buf;
        size_t n = 0;

        // iterate until the array is full
        while (n < SIZE)
        {
            size_t recvd = read(fd, buf.data(), SIZE - n);

            // check if error or socket closed
            if (recvd < 1)
            {
                {
                    std::lock_guard<std::mutex> lg(lck);
                    finished = true;
                }
                cv.notify_all();
                return;
            }
            n += recvd;
        }

        // push the array to the queue
        {
            std::lock_guard<std::mutex> lg(lck);
            recv_data.push(std::move(buf));
        }
        cv.notify_all();
    }
}

int socketbuf::underflow()
{
    {
        // try to pull a new array from the queue
        std::unique_lock<std::mutex> lg(lck);
        cv.wait(lg, [&]{ return finished || recv_data.size() > 0; });

        // check if error or socket closed
        if (finished && recv_data.size() == 0) return traits_type::eof();

        current_buf = std::move(recv_data.front());
        recv_data.pop();
    }

    // update streambuf pointers
    char *ptr = current_buf.data();
    setg(ptr, ptr, ptr + SIZE);
    return traits_type::to_int_type(*gptr());
}
