#pragma once

bool operator==(POINT lhs, POINT rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
bool operator!=(POINT lhs, POINT rhs) {return !(lhs == rhs);}

namespace detail
{
    namespace rx_completed
    {
        template<class T>
        struct tag {};
        template<class T>
        T unique_resource_invalid(tag<T>&&) { return nullptr; }
        template<class T>
        void unique_resource_reset(T resource, tag<T>&&) { resource->OnCompleted(); }
    }
}
template<typename T>
struct unique_rx_completed_factory
{
    typedef 
        UNIQUE_RESOURCE_NAMESPACE::unique_resource<detail::rx_completed::tag<T>>
    type;
private:
    ~unique_rx_completed_factory();
    unique_rx_completed_factory();
};

#if RXCPP_USE_VARIADIC_TEMPLATES
namespace detail {
template<class T, class Indices>
struct tuple_insert;
template<class T, size_t... DisptachIndices>
struct tuple_insert<T, rxcpp::util::tuple_indices<DisptachIndices...>> {
    const T* t;
    explicit tuple_insert(const T& targ) : t(&targ) {}
    template <class charT, class traits>
    std::basic_ostream<charT,traits>& operator()(std::basic_ostream<charT,traits>& os) const {
        os << "{";
        bool out[] = {((os << (DisptachIndices != 0 ? ", " : "") << std::get<DisptachIndices>(*t)), true)...};
        os << "}";
        return os;
    }
};}

template <class charT, class traits, class T, class Indices>
std::basic_ostream<charT,traits>& operator<<(std::basic_ostream<charT,traits>& os, const detail::tuple_insert<T, Indices>& ti) {
    return ti(os);
}

template<class T>
auto tuple_insert(const T& t) 
    -> decltype(detail::tuple_insert<T, typename rxcpp::util::make_tuple_indices<T>::type>(t)) {
    return      detail::tuple_insert<T, typename rxcpp::util::make_tuple_indices<T>::type>(t);
}
#endif

namespace detail {
template<class T>
struct time_insert {
    const T* t;
    explicit time_insert(const T& targ) : t(&targ) {}
    template <class charT, class traits>
    std::basic_ostream<charT,traits>& operator()(std::basic_ostream<charT,traits>& os) const {
        auto tt = std::chrono::system_clock::to_time_t(*t);
        auto tm = std::localtime(&tt);
        std::chrono::duration<double> fraq = *t - 
                                        std::chrono::system_clock::from_time_t(tt) +
                                        std::chrono::seconds(tm->tm_sec);
        os << std::put_time(tm, L"%H:%M:%S.") << fraq.count(); 
        return os;
    }
};}

template <class charT, class traits, class T>
std::basic_ostream<charT,traits>& operator<<(std::basic_ostream<charT,traits>& os, const detail::time_insert<T>& ti) {
    return ti(os);
}

template<class T>
auto time_insert(const T& t) 
    -> decltype(detail::time_insert<T>(t)) {
    return      detail::time_insert<T>(t);
}

struct Count {
    Count() : nexts(0), completions(0), errors(0), disposals(0) {}
    std::atomic<int> nexts;
    std::atomic<int> completions;
    std::atomic<int> errors;
    std::atomic<int> disposals;
};
template <class T>
std::shared_ptr<rxcpp::Observable<T>> Record(
    const std::shared_ptr<rxcpp::Observable<T>>& source,
    Count* count
    )
{
    return rxcpp::CreateObservable<T>(
        [=](std::shared_ptr<rxcpp::Observer<T>> observer)
        {
            rxcpp::ComposableDisposable cd;
            cd.Add(rxcpp::Disposable([=](){
                ++count->disposals;}));
            cd.Add(rxcpp::Subscribe(
                source,
            // on next
                [=](T element)
                {
                    ++count->nexts;
                    observer->OnNext(std::move(element));
                },
            // on completed
                [=]
                {
                    ++count->completions;
                    observer->OnCompleted();
                },
            // on error
                [=](const std::exception_ptr& error)
                {
                    ++count->errors;
                    observer->OnError(error);
                }));
            return cd;
        });
}

struct record {};
template<class T>
rxcpp::Binder<std::shared_ptr<rxcpp::Observable<T>>> rxcpp_chain(record&&, const std::shared_ptr<rxcpp::Observable<T>>& source, Count* count) {
    return rxcpp::from(Record(source, count));
}
