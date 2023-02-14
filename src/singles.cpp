
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
        uint64_t value
) {
    uint8_t data[event_size];
    uint64_t last_tt_value = 0;
    bool synced = false;

    std::vector<Single> sgls;
    TimeTag tt;

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
            sgls.emplace_back(data, tt);
        }
    }

    return sgls;
}

