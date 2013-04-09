// modernscratch.cpp : Defines the entry point for the application.
//

#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <ole2.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <vsstyle.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Ole32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <tuple>
#include <string>
#include <vector>
#include <list>

#include "cpprx/rx.hpp"
namespace rx=rxcpp;

#define UNIQUE_WINERROR_DEFINE_REPORTS
#define UNIQUE_HRESULT_DEFINE_REPORTS
#define LIBRARIES_NAMESPACE mylib
#include "libraries.h"
namespace l=LIBRARIES_NAMESPACE;


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


namespace rxmsg {
// allows message handling via Rx


struct message 
{
    typedef std::shared_ptr<rx::Subject<message>> Subject;
    typedef std::shared_ptr<rx::Observable<message>> Observable;
    typedef std::shared_ptr<rx::Observer<message>> Observer;

    HWND window;
    UINT id;
    WPARAM wParam;
    LPARAM lParam;
    l::wnd::dispatch_result* out;
};
bool operator==(const message& lhs, const message& rhs) {
    return lhs.window == rhs.window && lhs.id == rhs.id && lhs.wParam == rhs.wParam && lhs.lParam == rhs.lParam && lhs.out == rhs.out;
}
bool operator!=(const message& lhs, const message& rhs) {
    return !(lhs == rhs);
}

void set_lResult(l::wnd::dispatch_result* out, LRESULT r) {out->second = r;}
bool handled(l::wnd::dispatch_result* out) {return out->first;}
void set_handled(l::wnd::dispatch_result* out) {out->first = true;}

void set_lResult(const message& m, LRESULT r) {set_lResult(m.out, r);}
bool handled(const message& m) {return handled(m.out);}
void set_handled(const message& m) {set_handled(m.out);}


template<typename T> 
l::wnd::dispatch_result dispatch(
    const l::wnd::Context<T>& c, 
    message::Observer observer)
{
    l::wnd::dispatch_result result(false,0L);
    message rxmsg;

    rxmsg.window = c.window;
    rxmsg.lParam = c.lParam;
    rxmsg.wParam = c.wParam;
    rxmsg.id = c.message;
    rxmsg.out = &result;

    observer->OnNext(rxmsg);
    if (rxmsg.id == WM_NCDESTROY) 
    {
        observer->OnCompleted();
    }
    return result;
}

struct subclass_message 
{
    typedef std::shared_ptr<rx::Subject<subclass_message>> Subject;
    typedef std::shared_ptr<rx::Observable<subclass_message>> Observable;
    typedef std::shared_ptr<rx::Observer<subclass_message>> Observer;

    HWND window;
    UINT id;
    WPARAM wParam;
    LPARAM lParam;
    DWORD_PTR data;
    l::wnd::dispatch_result* out;
};
bool operator==(const subclass_message& lhs, const subclass_message& rhs) {
    return lhs.window == rhs.window && lhs.id == rhs.id && lhs.wParam == rhs.wParam && lhs.lParam == rhs.lParam && lhs.data == rhs.data && lhs.out == rhs.out;
}
bool operator!=(const subclass_message& lhs, const subclass_message& rhs) {
    return !(lhs == rhs);
}

void set_lResult(const subclass_message& m, LRESULT r) {set_lResult(m.out, r);}
bool handled(const subclass_message& m) {return handled(m.out);}
void set_handled(const subclass_message& m) {set_handled(m.out);}

struct subclass {};

typedef unique_rx_completed_factory<subclass_message::Subject>::type unique_subclass_message_completed;
typedef std::pair<l::wr::unique_remove_window_subclass, unique_subclass_message_completed> set_subclass_result;

set_subclass_result
set_window_subclass(HWND window, subclass_message::Subject subject, DWORD_PTR data = 0L)
{
    return set_subclass_result(l::wnd::set_window_subclass<subclass>(window, (UINT_PTR)(LPVOID)subject.get(), data), subject);
}

l::wnd::dispatch_result window_subclass_dispatch(const l::wnd::SubclassContext<subclass>& c, subclass&&)
{
    l::wnd::dispatch_result result(false,0L);
    subclass_message rxmsg;

    rx::Subject<subclass_message>* subject((rx::Subject<subclass_message>*)(LPVOID)c.id);

    rxmsg.window = c.window;
    rxmsg.lParam = c.lParam;
    rxmsg.wParam = c.wParam;
    rxmsg.data = c.data;
    rxmsg.id = c.message;
    rxmsg.out = &result;

    subject->OnNext(rxmsg);
    if (rxmsg.id == WM_NCDESTROY) 
    {
        subject->OnCompleted();
    }
    return result;
}

// every message dispatch is wrapped in this function.
template<typename Function>
l::wnd::dispatch_result window_message_error_contract(Function&& function, const l::wnd::SubclassContext<subclass>& c, subclass&&)
{
    try
    {
        return std::forward<Function>(function)(c);
    }
    catch(...)
    {
        rx::Subject<subclass_message>* subject((rx::Subject<subclass_message>*)(LPVOID)c.id);
        subject->OnError(std::current_exception());
        return l::wnd::dispatch_result(false, 0L);
    }
}

namespace detail {
template<class T, class Msg>
struct extract_args {
    extract_args() : m(nullptr) {}
    const Msg* m;
    T result;
    T operator,(long lResult) {if(!!m) {set_lResult(*m, lResult); set_handled(*m);} return std::move(result);}
};

namespace mark_handled {
    enum type {
        Yes,
        No
    };
}

template<class Msg>
struct handle_message;

template<>
struct handle_message<message> {
    const message* m;
    mark_handled::type mark;
    explicit handle_message(const message& m, mark_handled::type mark = mark_handled::Yes) : m(&m), mark(mark) {}
    template<class... T>
    detail::extract_args<std::tuple<T..., message>, message> operator()(HWND, T... t) {
        detail::extract_args<std::tuple<T..., message>, message> result;
        if (mark == mark_handled::Yes) {
            result.m = m;
        }
        result.result = std::make_tuple(t..., *m);
        return std::move(result);
    }
};

template<>
struct handle_message<subclass_message> {
    const subclass_message* m;
    mark_handled::type mark;
    explicit handle_message(const subclass_message& m, mark_handled::type mark = mark_handled::Yes) : m(&m), mark(mark) {}
    template<class... T>
    detail::extract_args<std::tuple<T..., subclass_message>, subclass_message> operator()(HWND, T... t) {
        detail::extract_args<std::tuple<T..., subclass_message>, subclass_message> result;
        if (mark == mark_handled::Yes) {
            result.m = m;
        }
        result.result = std::make_tuple(t..., *m);
        return std::move(result);
    }
};
}
template<class Msg>
detail::handle_message<Msg> crack_message(const Msg& m) {return detail::handle_message<Msg>(m, detail::mark_handled::No);}
template<class Msg>
detail::handle_message<Msg> handle_message(const Msg& m) {return detail::handle_message<Msg>(m);}

#define RXCPP_HANDLE_MESSAGE(id, m) HANDLE_ ## id ((m).window, (m).wParam, (m).lParam, rxmsg::handle_message(m))
#define RXCPP_CRACK_MESSAGE(id, m) HANDLE_ ## id ((m).window, (m).wParam, (m).lParam, rxmsg::crack_message(m))
// not tried yet.
//#define RXCPP_MESSAGE_IF(id, m, predicate) rxcpp::DispatchTuple(HANDLE_ ## id ((m).window, (m).wParam, (m).lParam, rxmsg::crack_message(m)), predicate)

template<UINT id>
struct messageId {
    template<class Msg>
    bool operator()(const Msg& m) const {
        return !handled(m) && m.id == id;
    }
};

namespace detail {
struct OriginFrom {};
struct ExtentFrom {};
struct LeftFrom {};
struct TopFrom {};
struct RightFrom {};
struct BottomFrom {};
struct WidthFrom {};
struct HeightFrom {};
struct CenterFrom {};
struct MiddleFrom {};
template<class Tag, class T>
struct From {
    From(T t) : t(std::move(t)) {}
    T t;
};
}
template<class ObservablePoint>
auto originFrom(ObservablePoint o) 
    ->     detail::From<detail::OriginFrom, ObservablePoint> {
    return detail::From<detail::OriginFrom, ObservablePoint>(std::move(o));}
template<class ObservableExtent>
auto extentFrom(ObservableExtent o) 
    ->     detail::From<detail::ExtentFrom, ObservableExtent> {
    return detail::From<detail::ExtentFrom, ObservableExtent>(std::move(o));}
template<class ObservableXCoordinate>
auto leftFrom(ObservableXCoordinate o) 
    ->     detail::From<detail::LeftFrom, ObservableXCoordinate> {
    return detail::From<detail::LeftFrom, ObservableXCoordinate>(std::move(o));}
template<class ObservableYCoordinate>
auto topFrom(ObservableYCoordinate o) 
    ->     detail::From<detail::TopFrom, ObservableYCoordinate> {
    return detail::From<detail::TopFrom, ObservableYCoordinate>(std::move(o));}
template<class ObservableXCoordinate>
auto rightFrom(ObservableXCoordinate o) 
    ->     detail::From<detail::RightFrom, ObservableXCoordinate> {
    return detail::From<detail::RightFrom, ObservableXCoordinate>(std::move(o));}
template<class ObservableYCoordinate>
auto bottomFrom(ObservableYCoordinate o) 
    ->     detail::From<detail::BottomFrom, ObservableYCoordinate> {
    return detail::From<detail::BottomFrom, ObservableYCoordinate>(std::move(o));}
template<class ObservableXExtent>
auto widthFrom(ObservableXExtent o) 
    ->     detail::From<detail::WidthFrom, ObservableXExtent> {
    return detail::From<detail::WidthFrom, ObservableXExtent>(std::move(o));}
template<class ObservableYExtent>
auto heightFrom(ObservableYExtent o) 
    ->     detail::From<detail::HeightFrom, ObservableYExtent> {
    return detail::From<detail::HeightFrom, ObservableYExtent>(std::move(o));}
template<class ObservableXCoordinate>
auto centerFrom(ObservableXCoordinate o) 
    ->     detail::From<detail::CenterFrom, ObservableXCoordinate> {
    return detail::From<detail::CenterFrom, ObservableXCoordinate>(std::move(o));}
template<class ObservableYCoordinate>
auto middleFrom(ObservableYCoordinate o) 
    ->     detail::From<detail::MiddleFrom, ObservableYCoordinate> {
    return detail::From<detail::MiddleFrom, ObservableYCoordinate>(std::move(o));}

template<class Tag>
struct measurement {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::Message Message;
    typedef typename traits::XCoordinate XCoordinate;
    typedef typename traits::YCoordinate YCoordinate;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;

    measurement() {}
    measurement(
        XCoordinate l, YCoordinate t,
        XCoordinate r, YCoordinate b) : 
        left(std::move(l)), top(std::move(t)),
        right(std::move(r)), bottom(std::move(b)) {}
    XCoordinate left;
    YCoordinate top;
    XCoordinate right;
    YCoordinate bottom;
    XExtent width() {return right - left;}
    YExtent height() {return bottom - top;}
    Point origin() {return Point(left, top);}
    Extent extent() {return Extent(width(), height());}
    XCoordinate center() {return left + (width() / 2);}
    YCoordinate middle() {return top + (height() / 2);}
};

template<class Tag>
bool operator==(const measurement<Tag>& l, const measurement<Tag>& r) {
    return l.left == r.left && l.top == r.top && l.right == r.right && l.bottom == r.bottom;
}

template<class Tag>
struct window_size : public std::enable_shared_from_this<window_size<Tag>> {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::Message Message;
    typedef typename traits::XCoordinate XCoordinate;
    typedef typename traits::YCoordinate YCoordinate;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef measurement<tag> measurement;
    typedef std::shared_ptr<rx::Observer<XCoordinate>> ObserverXCoordinate;
    typedef std::shared_ptr<rx::Observer<YCoordinate>> ObserverYCoordinate;
    typedef std::shared_ptr<rx::Observer<XExtent>> ObserverXExtent;
    typedef std::shared_ptr<rx::Observer<YExtent>> ObserverYExtent;
    typedef std::shared_ptr<rx::Observer<Point>> ObserverPoint;
    typedef std::shared_ptr<rx::Observer<Extent>> ObserverExtent;
    typedef std::shared_ptr<rx::Observable<XCoordinate>> ObservableXCoordinate;
    typedef std::shared_ptr<rx::Observable<YCoordinate>> ObservableYCoordinate;
    typedef std::shared_ptr<rx::Observable<XExtent>> ObservableXExtent;
    typedef std::shared_ptr<rx::Observable<YExtent>> ObservableYExtent;
    typedef std::shared_ptr<rx::Observable<Point>> ObservablePoint;
    typedef std::shared_ptr<rx::Observable<Extent>> ObservableExtent;
    typedef std::shared_ptr<rx::Observable<std::tuple<XCoordinate, YCoordinate, XCoordinate, YCoordinate>>> ObservableCoordinateBox;
    typedef std::shared_ptr<rx::BehaviorSubject<XCoordinate>> SubjectXCoordinate;
    typedef std::shared_ptr<rx::BehaviorSubject<YCoordinate>> SubjectYCoordinate;
    typedef std::shared_ptr<rx::BehaviorSubject<XExtent>> SubjectXExtent;
    typedef std::shared_ptr<rx::BehaviorSubject<YExtent>> SubjectYExtent;
    typedef std::shared_ptr<rx::BehaviorSubject<Point>> SubjectPoint;
    typedef std::shared_ptr<rx::BehaviorSubject<Extent>> SubjectExtent;

    window_size(HWND parent, HWND window, typename Message::Subject messages) : parent(parent), window(window), messages(rxcpp::observable(messages)), subscribed(0) {
        initSubjects();
        updateSubjects();
    }

    ObservablePoint origin() {
        return create(subjectOrigin);}
    ObservableExtent extent() {
        return create(subjectExtent);}

    ObservableXCoordinate left() {
        return create(subjectLeft);}
    ObservableYCoordinate top() {
        return create(subjectTop);}
    ObservableXCoordinate right() {
        return create(subjectRight);}
    ObservableYCoordinate bottom() {
        return create(subjectBottom);}

    ObservableXExtent width() {
        return create(subjectWidth);}
    ObservableYExtent height() {
        return create(subjectHeight);}

    ObservableXCoordinate center() {
        return create(subjectCenter);}
    ObservableYCoordinate middle() {
        return create(subjectMiddle);}

    measurement measure() {
        RECT client = {};
        if (!parent) {
            GetClientRect(window, &client);
        }
        else {
            GetWindowRect(window, &client);
            MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&client, 2);
        }
        measurement m(
            XCoordinate(client.left),
            YCoordinate(client.top),
            XCoordinate(client.right),
            YCoordinate(client.bottom)
        );
        return m;
    }

    template<class LTag, class LObservable, class... Tag, class... Observable> 
    std::shared_ptr<rx::Observable<measurement>> create_binding(const detail::From<LTag, LObservable>& l, const detail::From<Tag, Observable>&... f) {
        auto keepAlive = this->shared_from_this();
        return rx::CreateObservable<measurement>(
            [=](const std::shared_ptr<rx::Observer<measurement>>& observer) {
                sdBind.Dispose();
                sdBind.Set(rx::from(l.t)
                    .zip(f.t...)
                    .subscribe(rxcpp::MakeTupleDispatch(
                        [=](typename rx::observable_item<LObservable>::type lv, typename rx::observable_item<Observable>::type... fv) {
                            setposition<LTag, Tag...>(keepAlive, observer, lv, fv...);
                    })));
                return sdBind;
        });
    }

private:
    void initSubjects() {
        subjectOrigin = rx::CreateBehaviorSubject<Point>(Point());
        subjectExtent = rx::CreateBehaviorSubject<Extent>(Extent());
        subjectLeft = rx::CreateBehaviorSubject<XCoordinate>(XCoordinate());
        subjectTop = rx::CreateBehaviorSubject<YCoordinate>(YCoordinate());
        subjectRight = rx::CreateBehaviorSubject<XCoordinate>(XCoordinate());
        subjectBottom = rx::CreateBehaviorSubject<YCoordinate>(YCoordinate());
        subjectWidth = rx::CreateBehaviorSubject<XExtent>(XExtent());
        subjectHeight = rx::CreateBehaviorSubject<YExtent>(YExtent());
        subjectCenter = rx::CreateBehaviorSubject<XCoordinate>(XCoordinate());
        subjectMiddle = rx::CreateBehaviorSubject<YCoordinate>(YCoordinate());
    }
    template<class... Tag, class KeepAlive, class Observer, class... Value> 
    static void setposition(const KeepAlive& keepAlive, const Observer& observer, const Value&... fv) {
        auto m = keepAlive->measure();
        fixed state;
        bool sets[] = {(setter<Tag>::set(state, m, fv), true)...};
        observer->OnNext(m);
    }
    struct fixed {
        fixed() 
            : left(false)
            , top(false)
            , right(false)
            , bottom(false)
            , width(false)
            , height(false)
            , center(false)
            , middle(false)
        {}
        bool left;
        bool top;
        bool right;
        bool bottom;
        bool width;
        bool height;
        bool center;
        bool middle;
        bool leftFixed() {return left || (int(right) + width + center) > 1;}
        bool topFixed() {return top || (int(bottom) + height + middle) > 1;}
        bool rightFixed() {return right || (int(left) + width + center) > 1;}
        bool bottomFixed() {return bottom || (int(top) + height + middle) > 1;}
        bool widthFixed() {return width || (int(left) + right + center) > 1;}
        bool heightFixed() {return height || (int(top) + bottom + middle) > 1;}
        bool centerFixed() {return center || (int(left) + right + width) > 1;}
        bool middleFixed() {return middle || (int(top) + bottom + height) > 1;}
    };
    void updateSubjects() {
        auto m = measure();
        subjectOrigin->OnNext(m.origin());
        subjectExtent->OnNext(m.extent());
        subjectLeft->OnNext(m.left);
        subjectTop->OnNext(m.top);
        subjectRight->OnNext(m.right);
        subjectBottom->OnNext(m.bottom);
        subjectWidth->OnNext(m.width());
        subjectHeight->OnNext(m.height());
        subjectCenter->OnNext(m.center());
        subjectMiddle->OnNext(m.middle());
    }
    template<class Subject>
    typename rx::subject_observable<Subject>::type create(const Subject& subject) {
        auto keepAlive = this->shared_from_this();
        return rx::CreateObservable<typename rx::subject_item<Subject>::type>(
            [keepAlive, this, subject](const typename rx::subject_observer<Subject>::type& observer) {
                if (++subscribed == 1) {
                    sdMessages.Dispose();
                    sdMessages.Set(rx::from(messages)
                        .where(messageId<WM_WINDOWPOSCHANGED>())
                        .select([](const Message& m){
                            // expands to:
                            // return HANDLE_WM_WINDOWPOSCHANGED(m.window, m.wParam, m.lParam, rxmsg::HandleMessage(m));
                            return RXCPP_HANDLE_MESSAGE(WM_WINDOWPOSCHANGED, m);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([keepAlive](const LPWINDOWPOS pos, const Message&){
                                keepAlive->updateSubjects();}),
                        // on completed
                            [keepAlive](){
                                keepAlive->subjectOrigin->OnCompleted();
                                keepAlive->subjectExtent->OnCompleted();
                                keepAlive->subjectLeft->OnCompleted();
                                keepAlive->subjectRight->OnCompleted();
                                keepAlive->subjectTop->OnCompleted();
                                keepAlive->subjectBottom->OnCompleted();
                                keepAlive->subjectWidth->OnCompleted();
                                keepAlive->subjectHeight->OnCompleted();
                                keepAlive->subjectCenter->OnCompleted();
                                keepAlive->subjectMiddle->OnCompleted();}, 
                        // on error
                            [keepAlive](const std::exception_ptr& e){
                                keepAlive->subjectOrigin->OnError(e);
                                keepAlive->subjectExtent->OnError(e);
                                keepAlive->subjectLeft->OnError(e);
                                keepAlive->subjectRight->OnError(e);
                                keepAlive->subjectTop->OnError(e);
                                keepAlive->subjectBottom->OnError(e);
                                keepAlive->subjectWidth->OnError(e);
                                keepAlive->subjectHeight->OnError(e);
                                keepAlive->subjectCenter->OnError(e);
                                keepAlive->subjectMiddle->OnError(e);
                        }));
                }
                rx::ComposableDisposable cd;
                cd.Add(rx::Disposable([keepAlive](){
                    if(--keepAlive->subscribed == 0){
                        keepAlive->sdMessages.Dispose();}}));
                cd.Add(subject->Subscribe(observer));
                return cd;
            });
    }

    template<class Tag>
    struct setter;

    template<>
    struct setter<detail::LeftFrom> {
        static void set(fixed& f, measurement& m, XCoordinate x) {
            if (f.leftFixed()) {
                throw std::logic_error("already bound left.");}
            f.left = true;
            if (f.right) {m.left = x;}
            else if (f.center) {auto delta = m.center() - x; m.left = x; m.right = x + (delta*2);}
            else {m.right = x + m.width(); m.left = x;}
        }
    };
    template<>
    struct setter<detail::TopFrom> {
        static void set(fixed& f, measurement& m, YCoordinate y) {
            if (f.topFixed()) {
                throw std::logic_error("already bound top.");}
            f.top = true;
            if (f.bottom) {m.top = y;}
            else if (f.middle) {auto delta = m.middle() - y; m.top = y; m.bottom = y + (delta*2);}
            else {m.bottom = y + m.height(); m.top = y;}
        }
    };
    template<>
    struct setter<detail::RightFrom> {
        static void set(fixed& f, measurement& m, XCoordinate x) {
            if (f.rightFixed()) {
                throw std::logic_error("already bound right.");}
            f.right = true;
            if (f.left) {m.right = x;}
            else if (f.center) {auto delta = x - m.center(); m.right = x; m.left = x - (delta*2);}
            else {m.left = x - m.width(); m.right = x;}
        }
    };
    template<>
    struct setter<detail::BottomFrom> {
        static void set(fixed& f, measurement& m, YCoordinate y) {
            if (f.bottomFixed()) {
                throw std::logic_error("already bound bottom.");}
            f.bottom = true;
            if (f.top) {m.bottom = y;}
            else if (f.middle) {auto delta = y - m.middle(); m.bottom = y; m.top = y - (delta*2);}
            else {m.top = y - m.height(); m.bottom = y;}
        }
    };
    template<>
    struct setter<detail::WidthFrom> {
        static void set(fixed& f, measurement& m, XExtent cx) {
            if (f.widthFixed()) {
                throw std::logic_error("already bound width.");}
            f.width = true;
            if (f.left) {
                m.right = m.left + cx;}
            else if (f.right) {
                m.left = m.right - cx;}
            else { // act as if center is fixed even if it isn't
                auto delta = (m.width().c - cx.c) / 2; m.left += delta; m.right = m.left + cx;}
        }
    };
    template<>
    struct setter<detail::HeightFrom> {
        static void set(fixed& f, measurement& m, YExtent cy) {
            if (f.heightFixed()) {
                throw std::logic_error("already bound height.");}
            f.height = true;
            if (f.top) {
                m.bottom = m.top + cy;}
            else if (f.bottom) {
                m.top = m.bottom - cy;}
            else { // act as if middle is fixed even if it isn't
                auto delta = (m.height().c - cy.c) / 2; m.top += delta; m.bottom = m.top + cx;}
        }
    };
    template<>
    struct setter<detail::CenterFrom> {
        static void set(fixed& f, measurement& m, XCoordinate x) {
            if (f.centerFixed()) {
                throw std::logic_error("already bound center.");}
            f.center = true;
            if (f.left) {
                m.right = x + (x - m.left);}
            else if (f.right) {
                m.left = x - (m.right - x);}
            else { // act as if width is fixed even if it isn't
                auto w = m.width(); auto delta = w / 2; m.left = x - delta; m.right = m.left + w;}
        }
    };
    template<>
    struct setter<detail::MiddleFrom> {
        static void set(fixed& f, measurement& m, YCoordinate y) {
            if (f.middleFixed()) {
                throw std::logic_error("already bound middle.");}
            f.middle = true;
            if (f.top) {
                m.bottom = y + (y - m.top);}
            else if (f.bottom) {
                m.top = y - (m.bottom - y);}
            else { // act as if height is fixed even if it isn't
                auto h = m.height(); auto delta = h / 2; m.top = y - delta; m.bottom = m.top + h;}
        }
    };
    template<>
    struct setter<detail::OriginFrom> {
        static void set(fixed& f, measurement& m, Point p) {
            set(f, m, XCoordinate(p.p.x), detail::LeftFrom());
            set(f, m, YCoordinate(p.p.y), detail::TopFrom());}
    };
    template<>
    struct setter<detail::ExtentFrom> {
        static void set(fixed& f, measurement& m, Extent p) {
        set(f, m, XExtent(e.e.cx), detail::WidthFrom());
        set(f, m, YExtent(e.e.cy), detail::HeightFrom());}
    };

    typename Message::Observable messages;
    HWND parent;
    HWND window;
    std::atomic<int> subscribed;
    rx::SharedDisposable sdMessages;
    rx::SharedDisposable sdBind;
    SubjectPoint subjectOrigin;
    SubjectExtent subjectExtent;
    SubjectXCoordinate subjectLeft;
    SubjectYCoordinate subjectTop;
    SubjectXCoordinate subjectRight;
    SubjectYCoordinate subjectBottom;
    SubjectXExtent subjectWidth;
    SubjectYExtent subjectHeight;
    SubjectXCoordinate subjectCenter;
    SubjectYCoordinate subjectMiddle;
};

template<class Tag>
class stack {
public:
private:
    std::vector<std::shared_ptr<window_size<Tag>>> stacked;
};

}

namespace rxmsg{namespace traits{
    template<class XCoordinateArg>
    struct XCoordinate {
        XCoordinateArg c;
        XCoordinate() {c = 0;}
        explicit XCoordinate(XCoordinateArg carg) {c = carg;}
        XCoordinate(const XCoordinate& carg) : c(carg.c) {}
    };
    template<class YCoordinateArg>
    struct YCoordinate {
        YCoordinateArg c;
        YCoordinate() {c = 0;}
        explicit YCoordinate(YCoordinateArg carg) {c = carg;}
        YCoordinate(const YCoordinate& carg) : c(carg.c) {}
    };
    template<class XExtentArg>
    struct XExtent {
        XExtentArg c;
        XExtent() {c = 0;}
        explicit XExtent(XExtentArg carg) {c = carg;}
        XExtent(const XExtent& carg) : c(carg.c) {}
    };
    template<class YExtentArg>
    struct YExtent {
        YExtentArg c;
        YExtent() {c = 0;}
        explicit YExtent(YExtentArg carg) {c = carg;}
        YExtent(const YExtent& carg) : c(carg.c) {}
    };
    template<class XCoordinateArg, class YCoordinateArg, class PointArg>
    struct Point {
        typedef XCoordinate<XCoordinateArg> XCoordinate;
        typedef YCoordinate<YCoordinateArg> YCoordinate;
        PointArg p;
        Point() {p.x = 0; p.y = 0;}
        Point(XCoordinate x, YCoordinate y) {p.x = x.c; p.y = y.c;}
        explicit Point(PointArg parg) {p = parg;}
        Point(const Point& p) : p(p.p) {}
    };
    template<class XExtentArg, class YExtentArg, class ExtentArg>
    struct Extent {
        typedef XExtent<XExtentArg> XExtent;
        typedef YExtent<YExtentArg> YExtent;
        ExtentArg e;
        Extent() {e.cx = 0; e.cy = 0;}
        Extent(XExtent cx, YExtent cy) {e.cx = cx.c; e.cy = cy.c;}
        explicit Extent(ExtentArg earg) {e = earg;}
        Extent(const Extent& e) : e(e.e) {}
    };

    template<class XCoordinateArg>
    auto operator-(XCoordinate<XCoordinateArg> r, XCoordinate<XCoordinateArg> l) -> XExtent<decltype(r.c - l.c)> {return XExtent<decltype(r.c - l.c)>(r.c - l.c);}
    template<class XCoordinateArg>
    bool operator<(XCoordinate<XCoordinateArg> r, XCoordinate<XCoordinateArg> l) {return r.c < l.c;}
    template<class XCoordinateArg>
    bool operator==(XCoordinate<XCoordinateArg> r, XCoordinate<XCoordinateArg> l) {return r.c == l.c;}

    template<class YCoordinateArg>
    auto operator-(YCoordinate<YCoordinateArg> b, YCoordinate<YCoordinateArg> t) -> YExtent<decltype(b.c - t.c)> {return YExtent<decltype(b.c - t.c)>(b.c - t.c);}
    template<class YCoordinateArg>
    bool operator<(YCoordinate<YCoordinateArg> b, YCoordinate<YCoordinateArg> t) {return b.c < t.c;}
    template<class YCoordinateArg>
    bool operator==(YCoordinate<YCoordinateArg> b, YCoordinate<YCoordinateArg> t) {return b.c == t.c;}

    template<class XCoordinateArg, class XExtentArg>
    auto operator-(XCoordinate<XCoordinateArg> c, XExtent<XExtentArg> e) -> XCoordinate<XCoordinateArg> {return XCoordinate<XCoordinateArg>(c.c - e.c);}
    template<class XCoordinateArg, class XExtentArg>
    auto operator+(XCoordinate<XCoordinateArg> c, XExtent<XExtentArg> e) -> XCoordinate<XCoordinateArg> {return XCoordinate<XCoordinateArg>(c.c + e.c);}

    template<class YCoordinateArg, class YExtentArg>
    auto operator-(YCoordinate<YCoordinateArg> c, YExtent<YExtentArg> e) -> YCoordinate<YCoordinateArg> {return YCoordinate<YCoordinateArg>(c.c - e.c);}
    template<class YCoordinateArg, class YExtentArg>
    auto operator+(YCoordinate<YCoordinateArg> c, YExtent<YExtentArg> e) -> YCoordinate<YCoordinateArg> {return YCoordinate<YCoordinateArg>(c.c + e.c);}

    template<class XExtentArg>
    bool operator<(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {return l.c < r.c;}
    template<class XExtentArg>
    bool operator==(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {return l.c == r.c;}

    template<class YExtentArg>
    bool operator<(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {return l.c < r.c;}
    template<class YExtentArg>
    bool operator==(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {return l.c == r.c;}

    template<class XExtentArg>
    auto operator/(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {return XExtent<XExtentArg>(e.c/d);}
    template<class XExtentArg>
    auto operator*(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {return XExtent<XExtentArg>(e.c*d);}

    template<class YExtentArg>
    auto operator/(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {return YExtent<YExtentArg>(e.c/d);}
    template<class YExtentArg>
    auto operator*(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {return YExtent<YExtentArg>(e.c*d);}

    template<class XCoordinateArg, class YCoordinateArg, class XExtentArg, class YExtentArg, class PointArg, class ExtentArg, class MessageArg>
    struct window_traits_builder {
        typedef XCoordinate<XCoordinateArg> XCoordinate;
        typedef YCoordinate<YCoordinateArg> YCoordinate;
        typedef XExtent<XExtentArg> XExtent;
        typedef YExtent<YExtentArg> YExtent;
        typedef Point<XCoordinateArg, YCoordinateArg, PointArg> Point;
        typedef Extent<XExtentArg, YExtentArg, ExtentArg> Extent;
        typedef MessageArg Message;
    };

    struct OwnDefault {};
    window_traits_builder<int, int, int, int, POINT, SIZE, message> rx_window_traits(OwnDefault&&);

    struct SubclassDefault {};
    window_traits_builder<int, int, int, int, POINT, SIZE, subclass_message> rx_window_traits(SubclassDefault&&);
}}
using rxmsg::traits::rx_window_traits;

namespace rxtheme {

template<class Tag>
class theme_data {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::Message Message;

    theme_data(HWND w, std::wstring c, const typename Message::Observable& m) :
            window(w),
            classId(std::move(c)),
            messages(m)
    {
        auto loadThemeData = [this](){
            handle.reset();
            handle.reset(OpenThemeData(window, classId.c_str()));
            if (!handle) {
                throw std::runtime_error("theme data not loaded");}
        }

        rx::from(messages)
            .where(messageId<WM_THEMECHANGED>())
            .subscribe([loadThemeData](const Message& m){
                rxmsg::set_handled(m); loadThemeData()});

        loadThemeData();
    }

public:

    HWND window;
    std::wstring classId;
    typename Message::Observable messages;
    l::wr::unique_theme_data handle;

private:
};

template<class Tag>
struct label {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::Message Message;
    typedef rxmsg::measurement<tag> measurement;
    typedef std::shared_ptr<rx::BehaviorSubject<measurement>> MeasurementSubject;
    typedef std::shared_ptr<rx::Observable<measurement>> MeasurementObservable;

    label(HWND w, const typename Message::Observable& m) : 
        window(w), messages(m), theme(window, L"static", messages) {
    } 
    MeasurementObservable measure() {return rx::observable(measureSubject);}
    HWND window;
    typename Message::Observable messages;
    theme_data<tag> theme;
    MeasurementSubject measureSubject;
    measurement measureText() 
    {
        measurement result;
        HDC dc = GetDC(window);
        auto text = l::wnd::get_text(window);
        auto font = l::wnd::select_font(dc, GetFont(window));
        measurement = TextExtent(dc, text.c_str());
        return result;
    }
};

}

bool operator==(POINT lhs, POINT rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
bool operator!=(POINT lhs, POINT rhs) {return !(lhs == rhs);}

typedef std::queue<std::pair<std::string, std::exception_ptr>> Exceptions;

namespace RootWindow
{
    // tie the ADL methods together
    struct tag {};

    typedef
        l::wnd::Context<tag>
    Context;

    inline l::wr::unique_destroy_window CreateChildWindow(LPCWSTR className, LPCWSTR text, POINT p, HINSTANCE hinst, HWND parent)
    {
        unique_winerror winerror;
        l::wr::unique_destroy_window child;
        std::tie(winerror, child) = 
            l::wr::winerror_and_destroy_window(
                CreateWindowW(
                    className, text, 
                    WS_CHILD | WS_VISIBLE,
                    p.x, p.y, 20, 30, 
                    parent, NULL, 
                    hinst, 
                    NULL));

        if (!winerror || !child)
        {
            winerror.throw_if("CreateWindow failed");
        }
        return std::move(child);
    }

    struct label {
        typedef rxmsg::window_size<rxmsg::traits::SubclassDefault> window_size;
        typedef window_size::XCoordinate XCoordinate;
        typedef window_size::YCoordinate YCoordinate;
        typedef window_size::XExtent XExtent;
        typedef window_size::YExtent YExtent;
        typedef window_size::SubjectXCoordinate SubjectXCoordinate;
        typedef window_size::SubjectYCoordinate SubjectYCoordinate;

        label(std::wstring text, POINT p, HWND parent, HINSTANCE hInstance)
            : text(std::move(text))
            , window(CreateChildWindow(L"Static", this->text.c_str(), p, hInstance, parent))
            , messages(rx::CreateSubject<rxmsg::subclass_message>())
            , size(std::make_shared<window_size>(parent, window.get(), messages))
            , subclass(rxmsg::set_window_subclass(window.get(), messages))
        {}
        label(label&& o) 
            : text(std::move(o.text))
            , window(std::move(o.window))
            , messages(std::move(o.messages))
            , size(std::move(o.size))
            , subclass(std::move(o.subclass))
        {}
        label& operator=(label o) {
            using std::swap;
            swap(text,o.text);
            swap(window,o.window);
            swap(messages,o.messages);
            swap(size,o.size);
            swap(subclass,o.subclass);
        }

        std::wstring text;
        l::wr::unique_destroy_window window;
        rxmsg::subclass_message::Subject messages;
        std::shared_ptr<window_size> size;
        rxmsg::set_subclass_result subclass;
    };

    // note: no base class
    struct window
    {
        typedef rxmsg::window_size<rxmsg::traits::OwnDefault> top_size;
        typedef rxmsg::window_size<rxmsg::traits::SubclassDefault> subclass_size;
        window(HWND handle, CREATESTRUCT& cs)
            : root(handle)
            , exceptions(reinterpret_cast<Exceptions*>(cs.lpCreateParams))
            , text(rx::CreateSubject<std::wstring>())
            , messages(rx::CreateSubject<rxmsg::message>())
            , size(std::make_shared<top_size>(HWND_DESKTOP, handle, messages))
            , edit(CreateChildWindow(L"Edit", L"", POINT(), cs.hInstance, handle))
            , editMessages(rx::CreateSubject<rxmsg::subclass_message>())
            , editSize(std::make_shared<subclass_size>(handle, edit.get(), editMessages))
            , editSubclass(rxmsg::set_window_subclass(edit.get(), editMessages))
            , mouseX(rx::CreateBehaviorSubject<typename label::XCoordinate>(label::XCoordinate()))
            , mouseY(rx::CreateBehaviorSubject<typename label::YCoordinate>(label::YCoordinate()))
        {
            rx::from(editSize->create_binding(
                rxmsg::leftFrom(size->left()),
                rxmsg::topFrom(size->top()),
                rxmsg::rightFrom(size->right()),
                rxmsg::bottomFrom(rx::from(size->top()).select([](label::YCoordinate y){
                    return y + label::YExtent(30);}).publish())
            )).subscribe([this](subclass_size::measurement m){
                SetWindowPos(edit.get(), nullptr, m.left.c, m.top.c, m.width().c, m.height().c, SWP_NOOWNERZORDER);
                InvalidateRect(edit.get(), nullptr, true);
            });

            auto commands = rx::from(messages)
                .where(rxmsg::messageId<WM_COMMAND>())
                .select([this](const rxmsg::message& m){
                    // expands to:
                    // return HANDLE_WM_COMMAND(m.window, m.wParam, m.lParam, rxmsg::HandleMessage(m));
                    return RXCPP_HANDLE_MESSAGE(WM_COMMAND, m);})
                .publish();

            // edit text changed
            rx::from(commands)
                .where(rxcpp::MakeTupleDispatch([this](int , HWND hwndCtl, UINT codeNotify, const rxmsg::message&){
                    return codeNotify == EN_CHANGE && hwndCtl == edit.get();}))
                .subscribe(
                // on next
                    rxcpp::MakeTupleDispatch([this](int , HWND , UINT, const rxmsg::message&){
                        auto length = Edit_GetTextLength(edit.get());
                        std::wstring line;
                        line.reserve(length + 1);
                        line.resize(length);
                        Edit_GetText(edit.get(), &line[0], length + 1);
                        text->OnNext(line);
                    }),
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in edit changed: ", e));});

            // post quit message
            rx::from(messages)
                .where(rxmsg::messageId<WM_NCDESTROY>())
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    cd.Dispose();
                    PostQuitMessage(0);
                });

            // print client
            rx::from(messages)
                .where(rxmsg::messageId<WM_PRINTCLIENT>())
                .select([](const rxmsg::message& m) {
                    return std::make_tuple(m.window, reinterpret_cast<HDC>(m.wParam), m.out);})
                .subscribe(
                //on next
                    [this](const std::tuple<HWND, HDC, l::wnd::dispatch_result*>& m){
                        rxmsg::set_handled(std::get<2>(m));
                        PAINTSTRUCT ps = {};
                        ps.hdc = std::get<1>(m);
                        GetClientRect(std::get<0>(m), &ps.rcPaint);
                        rxmsg::set_lResult(
                            std::get<2>(m),
                            this->PaintContent(ps)
                        );
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in printclient: ", e));});

            // paint
            rx::from(messages)
                .where(rxmsg::messageId<WM_PAINT>())
                .subscribe(
                // on next
                    [this](const rxmsg::message& m){
                        set_handled(m);
                        PAINTSTRUCT ps = {};
                        BeginPaint(m.window, &ps);
                        l::wr::unique_gdi_end_paint ender(std::make_pair(m.window, &ps));
                        set_lResult(
                            m,
                            this->PaintContent(ps)
                        );
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in paint: ", e));});

            // disable erase background
            rx::from(messages)
                .where(rxmsg::messageId<WM_ERASEBKGND>())
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    set_lResult(m,TRUE);
                });

            auto worker = std::make_shared<rx::EventLoopScheduler>();
            auto mainFormScheduler = std::make_shared<rx::win32::WindowScheduler>();

            auto mouseDown = rx::from(messages)
                .where(rxmsg::messageId<WM_LBUTTONDOWN>())
                .publish();

            auto mouseUp = rx::from(messages)
                .where(rxmsg::messageId<WM_LBUTTONUP>())
                .publish();

            // note: distinct_until_changed is necessary; while 
            //  Winforms filters duplicate mouse moves, 
            //  user32 doesn't filter this for you: http://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx

            //this only produces mouse move events 
            //between an LButtonDown and the next LButtonUp
            rx::from(mouseDown)
                .select_many([=, this](const rxmsg::message&) {
                    return rx::from(messages)
                        .take_until(mouseUp)
                        .where(rxmsg::messageId<WM_MOUSEMOVE>())
                        .select([this](const rxmsg::message& m) {
                            // expands to:
                            // return HANDLE_WM_MOUSEMOVE(m.window, m.wParam, m.lParam, rxmsg::HandleMessage());
                            return RXCPP_CRACK_MESSAGE(WM_MOUSEMOVE, m);})
                        .distinct_until_changed()
                        .publish();})
                .subscribe(rxcpp::MakeTupleDispatch([=](int x, int y, UINT, const rxmsg::message&) {
                    mouseX->OnNext(label::XCoordinate(x)); mouseY->OnNext(label::YCoordinate(y));}));

            rx::from(text)
                .subscribe(
                // on next
                    [=](const std::wstring& msg){
                        // set up labels and query

                        cd.Dispose();
                        labels.clear();

                        auto left = rxcpp::observable(mouseX);
                        auto middle = rxcpp::observable(mouseY);

                        for (int i = 0; msg[i]; ++i)
                        {
                            POINT loc = {20 * i, 30};
                            auto& charlabel = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);

                            cd.Add(rx::from(charlabel.size->create_binding(
                                    rxmsg::middleFrom(rx::from(middle).select([&charlabel](label::YCoordinate y) -> label::YCoordinate {
                                        return std::max(label::YCoordinate(30 + 15), y);}).publish()), 
                                    rxmsg::leftFrom(left)))
                                .distinct_until_changed()
                                // delay on worker thread
                                .delay(std::chrono::milliseconds(100), worker)
                                .observe_on(mainFormScheduler)
                                .subscribe([&charlabel](subclass_size::measurement m){
                                    SetWindowPos(charlabel.window.get(), nullptr, m.left.c, m.top.c, m.width().c, m.height().c, SWP_NOOWNERZORDER);
                                    InvalidateRect(charlabel.window.get(), nullptr, true);
                                }));

                            left = charlabel.size->right();
                            middle = charlabel.size->middle();
                        }
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in label onmove stream: ", e));});

            auto msg = L"Time flies like an arrow";
            Edit_SetText(edit.get(), msg);
            text->OnNext(msg);
        }

        inline label& CreateLabelFromLetter(wchar_t c, POINT p, HINSTANCE hinst, HWND parent)
        {
            std::wstring text;
            text.append(&c, &c+1);

            labels.emplace_back(std::move(text), p, parent, hinst);
            return labels.back();
        }

        LRESULT PaintContent(PAINTSTRUCT& ps)
        {
            ps.rcPaint.top = std::max(0L, ps.rcPaint.top);
            l::wr::unique_gdi_brush gray(CreateSolidBrush(RGB(240,240,240)));
            FillRect(ps.hdc, &ps.rcPaint, gray.get());
            return 0;
        }

        HWND root;
        Exceptions* exceptions;
        std::shared_ptr<rx::Subject<std::wstring>> text;
        rxmsg::message::Subject messages;
        std::shared_ptr<rxmsg::window_size<rxmsg::traits::OwnDefault>> size;
        l::wr::unique_destroy_window edit;
        rxmsg::subclass_message::Subject editMessages;
        std::shared_ptr<rxmsg::window_size<rxmsg::traits::SubclassDefault>> editSize;
        rxmsg::set_subclass_result editSubclass;
        rx::ComposableDisposable cd;
        std::list<label> labels;
        label::SubjectXCoordinate mouseX;
        label::SubjectYCoordinate mouseY;
    };
}

//
// ADL traits must be declared before the typedef to build the window
//

l::wnd::traits_builder<RootWindow::window> window_class_traits(RootWindow::tag&&);

void window_class_register(PCWSTR windowClass, WNDCLASSEX* wcex, RootWindow::tag&&)
{
    wcex->hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex->lpszClassName = windowClass;
}

template<typename T>
std::pair<bool, LRESULT> window_class_dispatch(T t, const RootWindow::Context& context, RootWindow::tag&&)
{
    return rxmsg::dispatch(context, t->messages);
}

// every message dispatch is wrapped in this function.
template<typename Function>
std::pair<bool, LRESULT> window_message_error_contract(Function&& function, const RootWindow::Context& context, RootWindow::tag&&)
{
    try
    {
        return std::forward<Function>(function)(context);
    }
    catch(std::bad_alloc&&)
    {
        return std::make_pair(false, ERROR_OUTOFMEMORY);
    }
    catch(unique_winerror::exception&& e)
    {
        return std::make_pair(false, e.get());
    }
    catch(unique_hresult::exception&& e)
    {
        return std::make_pair(false, e.get());
    }
}

//
// build the window
//
typedef
    l::wnd::window_class<RootWindow::tag>
RootWindowClass;

int PASCAL
wWinMain(HINSTANCE hinst, HINSTANCE, LPWSTR, int nShowCmd)
{
    unique_hresult hr;

    hr.reset(CoInitialize(NULL));
    if (!hr)
    {
        return FALSE;
    }
    ON_UNWIND_AUTO([&]{CoUninitialize();});

    InitCommonControls();

    Exceptions exceptions;

    RootWindowClass::Register(L"Scratch");

    unique_winerror winerror;
    l::wr::unique_destroy_window window;

    std::tie(winerror, window) = 
        l::wr::winerror_and_destroy_window(
            CreateWindowW(
                L"Scratch", L"Scratch", 
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                NULL, NULL, 
                hinst, 
                (LPVOID)&exceptions));

    if (!winerror || !window)
    {
        return winerror.get();
    }

    ShowWindow(window.get(), nShowCmd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        while(!exceptions.empty()) {
            std::string context;
            try {
                std::exception_ptr next;
                std::tie(context, next) = exceptions.front();
                exceptions.pop();
                std::rethrow_exception(next);
            } catch (const std::exception& e) {
                context.append(e.what());
                MessageBoxA(window.get(), context.c_str(), "scratch exception", MB_OK);
            }
        }
    }
    return 0;
}

