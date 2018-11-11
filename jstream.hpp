#include <functional>
#include <optional>
#include <iostream>

namespace jstream
{
template <typename S, typename F>
class FilterStream;

template <typename S, typename F>
class TransformStream;

template <typename S, typename F>
class FlatStream;

template<typename S, typename F>
class PeekStream;

template <typename S>
class LimitStream;

template <typename CRTP>
class Stream
{
  private:
    auto &impl() { return *static_cast<CRTP *>(this); }
    auto next() { return impl().next(); }

  public:
    using base_type = Stream<CRTP>;

    template <typename F>
    constexpr auto filter(F &&f) { return FilterStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template <typename F>
    constexpr auto map(F &&f) { return TransformStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template <typename F>
    constexpr auto flatMap(F &&f) { return FlatStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template<typename F>
    constexpr auto peek(F &&f) { return PeekStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    constexpr auto limit(std::size_t n) { return LimitStream<CRTP>{impl(), n}; };

    template <typename F>
    constexpr void forEach(F &&f)
    {
        while (!empty())
            std::forward<F>(f)(*next());
    }

    constexpr std::size_t count()
    {
        std::size_t count = 0;
        while (!empty())
        {
            count++;
            next();
        }
        return count;
    }

    constexpr auto sum()
    {
        typename CRTP::value_type sum{};
        while (!empty())
            sum += *next();
        return sum;
    }

    template <typename F>
    constexpr bool allMatch(F &&f)
    {
        bool ret = true;
        while (!empty())
            ret &= std::forward<F>(f)(*next());
        return ret;
    }

    template <typename F>
    constexpr bool anyMatch(F &&f)
    {
        while (!empty())
            if (std::forward<F>(f)(*next()))
                return true;
        return false;
    }

    template <typename F>
    constexpr bool noneMatch(F &&f)
    {
        bool ret = true;
        while (!empty())
            ret &= !std::forward<F>(f)(*next());
        return ret;
    }

    constexpr bool empty() {
        return impl().empty();
    }
};

template <typename S, typename F>
class FilterStream : public Stream<FilterStream<S, F>>
{
  public:
    using next_type = typename S::next_type;
    using value_type = typename S::value_type;

    constexpr FilterStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr next_type next()
    {
        while (!empty()) {
            next_type n = _stream.next();
            if (_f(*n))
                return n;
        }
        return nullptr;
    }

    constexpr bool empty() { return _stream.empty(); } // ??? TODO: I don't think it's that easy

  private:
    S &_stream;
    F _f;
};

template <typename S, typename F>
class TransformStream : public Stream<TransformStream<S, F>>
{
  public:
    using next_type = std::invoke_result_t<F, decltype(* typename S::next_type{})> *;
    using value_type = std::remove_cv_t<std::remove_pointer_t<next_type>>;

    constexpr TransformStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr next_type next()
    {
        if (!empty()) {
            _currentElement = _f(*_stream.next());
            return &_currentElement;
        } else {
            return nullptr;
        }
    }

    constexpr bool empty() { return _stream.empty(); }

  private:
    S &_stream;
    F _f;
    value_type _currentElement;
};

template <typename S, typename F>
class FlatStream : public Stream<FlatStream<S, F>>
{
  public:
    using substream_next_type = std::invoke_result_t<decltype(&S::next), S*>;
    using flat_stream_type = std::invoke_result_t<F, decltype(*substream_next_type{})>;
    using next_type = std::invoke_result_t<decltype(&flat_stream_type::next), flat_stream_type*>;
    using value_type = std::remove_cv_t<std::remove_pointer_t<next_type>>;

    constexpr FlatStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr next_type next(bool force = false)
    {
        if (!_currentFlatStream || force)
        {
            if (!_stream.empty())
                _currentFlatStream = _f(*_stream.next());
            else
                return nullptr;
        }
        return _currentFlatStream->empty() ? next(true) : _currentFlatStream->next();
    }

    constexpr bool empty() { return _stream.empty() && (!_currentFlatStream || _currentFlatStream->empty()); }

  private:
    S &_stream;
    F _f;
    std::optional<flat_stream_type> _currentFlatStream;
};

template<typename S, typename F>
class PeekStream : public Stream<PeekStream<S, F>>
{
  public:
    using next_type = typename S::next_type;
    using value_type = typename S::value_type;

    constexpr PeekStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr next_type next() {
        next_type n = _stream.next();
        if (n)
            _f(*n);
        return n;
    }

    constexpr bool empty() { return _stream.empty(); }

  private:
    S &_stream;
    F _f;
};

template <typename S>
class LimitStream : public Stream<LimitStream<S>>
{
  public:
    using next_type = typename S::next_type;
    using value_type = typename S::value_type;

    constexpr LimitStream(S &s, std::size_t n) : _stream(s), _n(n) {}

    constexpr next_type next()
    {
        if (_n > 0 && !_stream.empty())
        {
            _n--;
            return _stream.next();
        }
        return nullptr;
    }

    constexpr bool empty() { return _n <= 0 || _stream.empty(); }

  private:
    S &_stream;
    std::size_t _n;
};

template <typename InputIt>
class IteratorStream : public Stream<IteratorStream<InputIt>>
{
  public:
    using next_type = typename std::iterator_traits<InputIt>::pointer;
    using value_type = typename std::iterator_traits<InputIt>::value_type;

    constexpr IteratorStream(InputIt begin, InputIt end) : _begin(begin), _end(end) {}

    constexpr bool empty() { return _begin == _end; }

    constexpr next_type next() { return &*_begin++; }

  private:
    InputIt _begin;
    InputIt _end;
};

template <typename C>
class ContainerStream : public IteratorStream<decltype(C().begin())>
{
  public:
    constexpr ContainerStream(C &c) : IteratorStream<decltype(C().begin())>(c.begin(), c.end()) {}
};

template <typename T, std::size_t N>
class ArrayStream : public IteratorStream<T *>
{
  public:
    constexpr ArrayStream(T (&array)[N]) : IteratorStream<T *>(std::begin(array), std::end(array)) {}
};

template<typename C>
auto of(C &c) { return ContainerStream<C>{c}; }

template<typename T, std::size_t N>
auto of(T (&arr)[N]) { return ArrayStream<T, N>{arr}; }

template<typename InputIt>
auto of(InputIt beg, InputIt end) { return IteratorStream<InputIt>{beg, end}; } 

} // namespace jstream