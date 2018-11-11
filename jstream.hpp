#include <functional>
#include <optional>

namespace jstream
{
template <typename S, typename F>
class FilterStream;

template <typename S, typename F>
class TransformStream;

template <typename S, typename F>
class FlatStream;

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

    constexpr auto limit(std::size_t n) { return LimitStream<CRTP>{impl(), n}; };

    template <typename F>
    constexpr void forEach(F &&f)
    {
        auto n = next();
        while (n)
        {
            std::forward<F>(f)(n->get());
            n = next();
        }
    }

    constexpr std::size_t count()
    {
        std::size_t count = 0;
        while (next())
            count++;
        return count;
    }

    constexpr int sum()
    {
        typename CRTP::value_type sum{};
        for (auto n = next(); n; n = next())
            sum += n->get();
        return sum;
    }

    template <typename F>
    constexpr bool allMatch(F &&f)
    {
        bool ret = true;
        for (auto n = next(); n; n = next())
            ret &= std::forward<F>(f)(n->get());
        return ret;
    }

    template <typename F>
    constexpr bool anyMatch(F &&f)
    {
        for (auto n = next(); n; n = next())
            if (std::forward<F>(f)(n->get()))
                return true;
        return false;
    }

    template <typename F>
    constexpr bool noneMatch(F &&f)
    {
        bool ret = true;
        for (auto n = next(); n; n = next())
            ret &= !std::forward<F>(f)(n->get());
        return ret;
    }
};

template <typename S, typename F>
class FilterStream : public Stream<FilterStream<S, F>>
{
  public:
    using true_type = typename S::true_type;
    using value_type = typename S::value_type;
    using wrapper_type = typename S::wrapper_type;

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

  private:
    S &_stream;
    F _f;
};

template <typename S, typename F>
class TransformStream : public Stream<TransformStream<S, F>>
{
  public:
    using true_type = typename S::true_type;
    using value_type = typename S::value_type;
    using wrapper_type = typename S::wrapper_type;

    constexpr TransformStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next()
    {
        auto n = _stream.next();

        return n ? std::optional{_f(*n)} : std::nullopt;
    }

  private:
    S &_stream;
    F _f;
};

template <typename InputIt>
class IteratorStream : public Stream<IteratorStream<InputIt>>
{
  public:
    using true_type = typename std::iterator_traits<InputIt>::reference;
    using value_type = typename std::iterator_traits<InputIt>::value_type;
    using wrapper_type = std::optional<std::reference_wrapper<std::remove_reference_t<true_type>>>;

    constexpr IteratorStream(InputIt begin, InputIt end) : _begin(begin), _end(end) {}

    constexpr auto next()
    {
        return _begin != _end
                   ? std::optional{std::ref(*_begin++)}
                   : std::nullopt;
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
            _currentElement = _stream.next();
            if (_currentElement)
                _currentSubStream = _f(_currentElement->get());
            else
                return std::nullopt;
        }
        auto subN = _currentSubStream->next();
        return subN ? subN : next(true);
    }

  private:
    S &_stream;
    F _f;
    typename S::wrapper_type _currentElement;
    std::optional<substream_type> _currentSubStream;
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

  private:
    S &_stream;
    std::size_t _n;
};

template<typename C>
auto of(C &c) { return ContainerStream<C>{c}; }

template<typename T, std::size_t N>
auto of(T (&arr)[N]) { return ArrayStream<T, N>{arr}; }

template<typename InputIt>
auto of(InputIt beg, InputIt end) { return IteratorStream<InputIt>{beg, end}; } 

} // namespace jstream