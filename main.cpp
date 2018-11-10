#include "jstream.hpp"
#include <iostream>

int main() {
    int const array[][10] = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}        
    };
    auto stream = jstream::ArrayStream(array);

    return stream
            .flatMap([](int const (&arr)[10]) {
                return jstream::ArrayStream(arr);
            })
            .sum();
}