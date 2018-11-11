#include "jstream.hpp"
#include <iostream>
#include <array>
#include <cxxabi.h>

char const *__demangle(char const *name) {
    int status = 0;
    return abi::__cxa_demangle(name, 0, 0, &status);
}

template<typename T>
void dump() {
    int status;
    auto& stream = typeid(T);
    auto& base = typeid(typename std::remove_reference_t<T>::base_type);
    std::cout << "Stream: " << __demangle(stream.name()) << ": " << __demangle(base.name()) << '\n';

    auto& true_type = typeid(typename std::remove_reference_t<T>::true_type);
    std::cout << "    - true_type:    " << __demangle(true_type.name()) << '\n';

    auto& value_type = typeid(typename std::remove_reference_t<T>::value_type);
    std::cout << "    - value_type:   " << __demangle(value_type.name()) << '\n';

    auto& wrapper_type = typeid(typename std::remove_reference_t<T>::wrapper_type);
    std::cout << "    - wrapper_type: " << __demangle(wrapper_type.name()) << '\n';    
}

template<typename T>
void dump(T &&t) { dump<T>(); }

int main() {
    std::array<std::array<std::array<int, 10>, 2>, 2> array{{
        {{
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        }},
        {{
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        }}
    }};
    jstream::of(array)
    .flatMap([](std::array<std::array<int, 10>, 2> &arr) { return jstream::of(arr); })
    .peek([](auto const &array) {
        static int i = 0;
        std::cout << "Peeked: " << i++ << '\n';
    })
    .flatMap([](std::array<int, 10> &arr) { return jstream::of(arr); })
    .limit(3)
    .peek([](int i) {
        std::cout << "Peeked after limit: " << i << '\n';
    })
    .map([](int i) { return i * 10; })
    .peek([](int i) {
        std::cout << "Peeked after map: " << i << '\n';
    })
    .filter([](int i) { return i > 10; })
    .forEach([](int i) {
        std::cout << i << ", ";
    });
    return 0;
}