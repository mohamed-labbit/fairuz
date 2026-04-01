#ifndef PAIR_HPP
#define PAIR_HPP

#include "arena.hpp"

namespace fairuz {

template<typename T, typename S = T>
struct Fa_Pair {
    T first;
    S second;
};

template <typename T, typename S = T>
static Fa_Pair<T, S>  Fa_makePair(T f, S s) {
    return fairuz::get_allocator().allocate_object<Fa_Pair<T, S>>(f, s);
}

} // namespace fairuz

#endif // PAIR_HPP