#include <array>
#include <utility>
#include <iostream>
#include <optional>
#include <string_view>

template<typename S, typename F>
class FilterStream;

template<typename S, typename F>
class TransformStream;

template<typename S, typename F>
class FlatStream;

template<typename CRTP>
class Stream
{
private:
    auto& impl() { return *static_cast<CRTP*>(this); }
    auto next() { return impl().next(); }

public:
    template<typename F>
    constexpr auto filter(F&& f) { return FilterStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template<typename F>
    constexpr auto map(F&& f) { return TransformStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template<typename F>
    constexpr auto flatMap(F&& f) { return FlatStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template<typename F>
    constexpr void forEach(F&& f) {
        auto n = next();
        while (n) {
            std::forward<F>(f)(*n);
            n = next();
        }
    }

    constexpr std::size_t count() {
        std::size_t count = 0;
        while (next())
            count++;
        return count;
    }

    constexpr int sum() {
        int sum = 0;
        for (auto n = next(); n; n = next())
            sum += *n;
        return sum;
    }
};

template<typename S, typename F>
class FilterStream : public Stream<FilterStream<S, F>>
{
public:
    constexpr FilterStream(S& s, F f) : _stream(s), _f(f) {}

    constexpr auto next() {
        auto n = _stream.next();
        while (n && !_f(*n)) {
            n = _stream.next();
        }
        return n;
    }

private:
    S& _stream;
    F _f;
};

template<typename S, typename F>
class TransformStream : public Stream<TransformStream<S, F>>
{
public:
    constexpr TransformStream(S& s, F f) : _stream(s), _f(f) {}

    constexpr auto next() {
        auto n = _stream.next();

        return n ? std::optional{_f(*n)} : std::nullopt;
    }

private:
    S& _stream;
    F _f;
};

template<typename InputIt>
class IteratorStream : public Stream<IteratorStream<InputIt>>
{
public:
    constexpr IteratorStream(InputIt begin, InputIt end) : _begin(begin), _end(end) {}

    constexpr auto next() {
        return _begin != _end ? std::optional{*_begin++} : std::nullopt;
    }

private:
    InputIt _begin;
    InputIt _end;
};

template<typename C>
class ContainerStream : public IteratorStream<decltype(C().begin())>
{
public:
    constexpr ContainerStream(C& c) : IteratorStream<decltype(C().begin())>(c.begin(), c.end()) {}
};

template<typename S, typename F>
class FlatStream : public Stream<FlatStream<S, F>>
{
private:
    S& _stream;
    F _f;
    decltype(_stream.next()) __c;
    std::optional<decltype(_f(*_stream.next()))> _next;

public:
    constexpr FlatStream(S& s, F f) : _stream(s), _f(f) {}

    constexpr auto next(bool force = false) -> decltype(_next->next()) {
        if (!_next || force) {
            __c = _stream.next();
            if (__c)
                _next = _f(*__c);
            else
                return std::nullopt;
        }
        auto subN = _next->next();
        return subN ? subN : next(true);
    }
};

int main() {
    std::string const s{"Hello World!"};
    
    auto c = ContainerStream(s)
    .filter([](char c) { return c != 'o'; })
    .filter([](char c) { return std::islower(c); })
    .map([](char c) { return std::toupper(c); })
    .count();
    std::cout << '"' << s << '"' << ".filter(is not 'o').filter(islower).map(toupper).count(): " << c << '\n';

    using namespace std::literals;
    auto words = {"Hello"sv, "World!"sv};
    auto wcw = ContainerStream(words)
    .map([](std::string_view s) { std::cout << s << ' '; return s; })
    .count();
    std::cout << '\n' << wcw << '\n';

    auto wcc = ContainerStream(words)
    .flatMap([](std::string_view s) { return ContainerStream(s); })
    .count();
    std::cout << wcc << "\n\n";

    auto ints = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto sum = ContainerStream(ints)
    .filter([](int i) { return i % 2 == 0; })
    .sum();
    std::cout << sum << '\n';

    std::array<std::array<int, 6>, 2> other_ints{{
        {0, 1, 2, 3, 4, 5},
        {6, 7, 8, 9, 10, 11}
    }};
    auto other_sum = ContainerStream(other_ints)
    .flatMap([](std::array<int, 6> const& array) { return ContainerStream(array); })
    .filter([](int i) { return i % 2 == 0; })
    .sum();
    std::cout << other_sum << '\n';
    return 0;
}