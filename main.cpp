#include "jstream.hpp"
#include <iostream>

int main() {
    int array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto stream = jstream::ArrayStream(array);

    std::cout << (void*)&array << '\n';
    stream.forEach([](int& elt) {
        std::cout << (void*)&elt << '\n';
    });

    return stream.sum();
}