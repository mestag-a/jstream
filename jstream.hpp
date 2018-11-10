#include <functional>
#include <optional>

namespace jstream
{
/*
** next() should return std::optional<std::reference_wrapper<T>>
*/

template <typename S, typename F>
class FilterStream;

template <typename S, typename F>
class TransformStream;

template <typename S, typename F>
class FlatStream;

template <typename CRTP>
class Stream
{
private:
    auto &impl() { return *static_cast<CRTP *>(this); }
    auto next() { return impl().next(); }

  public:
    template <typename F>
    constexpr auto filter(F &&f) { return FilterStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template <typename F>
    constexpr auto map(F &&f) { return TransformStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template <typename F>
    constexpr auto flatMap(F &&f) { return FlatStream<CRTP, F>{impl(), std::forward<F>(f)}; }

    template <typename F>
    constexpr void forEach(F &&f)
    {
        auto n = next();
        while (n)
        {
            std::forward<F>(f)(*n);
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
        int sum = 0;
        for (auto n = next(); n; n = next())
            sum += n->get();
        return sum;
    }
};

template <typename S, typename F>
class FilterStream : public Stream<FilterStream<S, F>>
{
  public:
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
  private:
    S &_stream;
    F _f;
    decltype(_stream.next()) __c;
    std::optional<decltype(_f(*_stream.next()))> _next;

  public:
    constexpr FlatStream(S &s, F f) : _stream(s), _f(f) {}

    constexpr auto next(bool force = false) -> decltype(_next->next())
    {
        if (!_next || force)
        {
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
} // namespace jstream