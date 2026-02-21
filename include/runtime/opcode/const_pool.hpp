#pragma once

namespace mylang {
namespace runtime {

struct ConstPool {
    static constexpr int POOL_SIZE = 256;

    int64_t int_constant[POOL_SIZE];
    double float_constant[POOL_SIZE];

    uint8_t int_size = 0;
    uint8_t float_size = 0;

    size_t push(double const v)
    {
        float_constant[float_size++] = v;
        return float_size - 1;
    }

    size_t push(int64_t const v)
    {
        int_constant[int_size++] = v;
        return int_size - 1;
    }
};

}
}
