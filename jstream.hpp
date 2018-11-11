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
            sum += next()->get();
        return sum;
    }

    template <typename F>
    constexpr bool allMatch(F &&f)
    {
        bool ret = true;
        while (!empty())
            ret &= std::forward<F>(f)(next()->get());
        return ret;
    }

    template <typename F>
    constexpr bool anyMatch(F &&f)
    {
        while (!empty())
            if (std::forward<F>(f)(next()->get()))
                return true;
        return false;
    }

    template <typename F>
    constexpr bool noneMatch(F &&f)
    {
        bool ret = true;
        while (!empty())
            ret &= !std::forward<F>(f)(next()->get());
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
    // using true_type = typename S::true_type;
    // using value_type = typename S::value_type;
    // using wrapper_type = typename S::wrapper_type;

    constexpr FilterStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next()
    {
        auto n = _stream.next();
        while (n && !_f(*n))
        {
            n = _stream.next();
        }
        return n;
    }

    constexpr auto empty() { return _stream.empty(); }

  private:
    S &_stream;
    F _f;
};

template <typename S, typename F>
class TransformStream : public Stream<TransformStream<S, F>>
{
  public:
    using substream_next_type = std::invoke_result_t<decltype(&S::next), S*>;
    using true_type = std::invoke_result_t<F, decltype(*substream_next_type{})>;

    constexpr TransformStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next() -> true_type*
    {
        if (!empty()) {
            _currentElement = _f(*_stream.next());
            return &_currentElement;
        } else {
            return nullptr;
        }
    }

    constexpr auto empty() { return _stream.empty(); }

  private:
    S &_stream;
    F _f;
    true_type _currentElement;
};

template <typename S, typename F>
class FlatStream : public Stream<FlatStream<S, F>>
{
  public:
    using substream_type = typename std::invoke_result_t<F, typename S::true_type>;
    using true_type = typename substream_type::true_type;
    using value_type = std::remove_cv_t<true_type>;
    using wrapper_type = std::optional<std::reference_wrapper<std::remove_reference_t<true_type>>>;

    constexpr FlatStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next(bool force = false) -> wrapper_type
    {
        if (!_currentSubStream || force)
        {
            auto _currentElement = _stream.next();
            if (_currentElement)
                _currentSubStream = _f(_currentElement->get());
            else
                return std::nullopt;
        }
        auto subN = _currentSubStream->next();
        return subN ? subN : next(true);
    }

    constexpr auto empty() { return _stream.empty() && (!_currentSubStream || _currentSubStream->empty()); }

  private:
    S &_stream;
    F _f;
    std::optional<substream_type> _currentSubStream;
};

template<typename S, typename F>
class PeekStream : public Stream<PeekStream<S, F>>
{
  public:
    using true_type = typename S::true_type;
    using value_type = typename S::value_type;
    using wrapper_type = typename S::wrapper_type;

    constexpr PeekStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next() {
        auto n = _stream.next();

        if (n)
            _f(n->get());
        return n;
    }

    constexpr auto empty() { return _stream.empty(); }

  private:
    S &_stream;
    F _f;
};

template <typename S>
class LimitStream : public Stream<LimitStream<S>>
{
  public:
    using true_type = typename S::true_type;
    using value_type = typename S::value_type;
    using wrapper_type = typename S::wrapper_type;

    constexpr LimitStream(S &s, std::size_t n) : _stream(s), _n(n) {}

    constexpr auto next()
    {
        if (_n > 0)
        {
            _n--;
            return _stream.next();
        }
        return wrapper_type{};
    }

    constexpr auto empty() { return _n <= 0 || _stream.empty(); }

  private:
    S &_stream;
    std::size_t _n;
};

template <typename InputIt>
class IteratorStream : public Stream<IteratorStream<InputIt>>
{
  public:
    using true_type = typename std::iterator_traits<InputIt>::reference;
    using value_type = typename std::iterator_traits<InputIt>::value_type;
    using wrapper_type = std::optional<std::reference_wrapper<std::remove_reference_t<true_type>>>;

    constexpr IteratorStream(InputIt begin, InputIt end) : _begin(begin), _end(end) {}

    constexpr bool empty() { return _begin == _end; }

    constexpr auto next()
    {
        return _begin++;
    }

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