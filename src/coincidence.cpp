
#include "../include/coincidence.h"
#include <numeric>

CoincidenceData::CoincidenceData(const Single &a, const Single &b, bool isprompt)
{
    // Event a is earlier (lower abstime), b is later (greater abstime)
    // Event 1 has the lower block number, 2 has the higher

    const auto &[ev1, ev2] = a.blk < b.blk ?
        std::tie(a, b) : std::tie(b, a);

    SingleData sd1(ev1), sd2(ev2);

    // record absolute time in incr. of 100 ms
    uint64_t t = a.abs_time;
    t /= (TimeTag::clks_per_tt * 100);
    abstime(t);

    int8_t dt = ev1.abs_time - ev2.abs_time;
    tdiff(isprompt, dt);

    blk(ev1.blk, ev2.blk);
    e_aF(sd1.eF);
    e_aR(sd1.eR);
    e_bF(sd2.eF);
    e_bR(sd2.eR);
    x_a(sd1.x);
    y_a(sd1.y);
    x_b(sd2.x);
    y_b(sd2.y);
}

Coincidences CoincidenceData::sort(
        const std::vector<Single> &singles
) {
    Coincidences coin;

    for (auto a = singles.begin(), e = singles.end(); a != e; ++a)
    {
        // a
        // |*** prompts ***| ----------- |*** delays ***| ---->
        // |<--- width --->|
        // |<------------ delay -------->|
        auto dend = a->abs_time + delay + width;
        for (auto b = a + 1; b != e && b->abs_time < dend; ++b)
        {
            auto dt = b->abs_time - a->abs_time;
            bool isprompt = dt < width, isdelay = dt >= delay;
            if ((isprompt || isdelay) && a->valid_module(b->mod))
                coin.emplace_back(*a, *b, isprompt);
        }
    }

    return coin;
}

Coincidences CoincidenceData::sort_span(
        std::vector<std::vector<Single>> all_singles
) {
    size_t n = 0, offset = 0;
    for (const auto &sgl : all_singles)
        n += sgl.size();

    std::vector<Single> singles(n);
    for (const auto &sgl : all_singles)
    {
        auto size = sgl.size() * sizeof(Single);
        std::memcpy((char*)singles.data() + offset, (char*)sgl.data(), size);
        offset += size;
    }

    std::sort(singles.begin(), singles.end());
    return CoincidenceData::sort(singles);
}
