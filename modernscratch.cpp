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

    template<class Tag>
    struct operators {
        typedef Tag tag;
        typedef decltype(rx_measurement_traits(tag())) traits;
        typedef typename traits::Message Message;
        typedef typename traits::XPoint XPoint;
        typedef typename traits::YPoint YPoint;
        typedef typename traits::XExtent XExtent;
        typedef typename traits::YExtent YExtent;
        typedef typename traits::Point Point;
        typedef typename traits::Extent Extent;
        typedef typename traits::Rect Rect;
        typedef typename traits::Measurement Measurement;

        template<class Message>
        static auto SelectClientMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(messageId<WM_WINDOWPOSCHANGED>())
                        .select([](const Message& msg){
                            // expands to:
                            // return HANDLE_WM_WINDOWPOSCHANGED(m.window, m.wParam, m.lParam, rxmsg::HandleMessage(m));
                            return RXCPP_HANDLE_MESSAGE(WM_WINDOWPOSCHANGED, msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS pos, const Message& msg){
                                RECT client = {};
                                GetClientRect(msg.window, &client);
                                Measurement m(
                                    XPoint(client.left),
                                    YPoint(client.top),
                                    XPoint(client.right),
                                    YPoint(client.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

        template<class Message>
        static auto SelectScreenMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(messageId<WM_WINDOWPOSCHANGED>())
                        .select([](const Message& msg){
                            // expands to:
                            // return HANDLE_WM_WINDOWPOSCHANGED(m.window, m.wParam, m.lParam, rxmsg::HandleMessage(m));
                            return RXCPP_HANDLE_MESSAGE(WM_WINDOWPOSCHANGED, msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS pos, const Message& msg){
                                RECT screen = {};
                                GetWindowRect(msg.window, &screen);
                                Measurement m(
                                    XPoint(screen.left),
                                    YPoint(screen.top),
                                    XPoint(screen.right),
                                    YPoint(screen.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

        template<class Message>
        static auto SelectParentMeasurement(const std::shared_ptr<rx::Observable<Message>>& source) 
            -> std::shared_ptr<rx::Observable<Measurement>> {
            return rx::CreateObservable<Measurement>(
                [=](const std::shared_ptr<rx::Observer<Measurement>>& observer) {
                    return rx::from(source)
                        .where(messageId<WM_WINDOWPOSCHANGED>())
                        .select([](const Message& msg){
                            // expands to:
                            // return HANDLE_WM_WINDOWPOSCHANGED(m.window, m.wParam, m.lParam, rxmsg::HandleMessage(m));
                            return RXCPP_HANDLE_MESSAGE(WM_WINDOWPOSCHANGED, msg);})
                        .subscribe(
                        // on next
                            rxcpp::MakeTupleDispatch([=](const LPWINDOWPOS pos, const Message& msg){
                                RECT screen = {};
                                GetWindowRect(msg.window, &screen);
                                auto parent = GetAncestor(msg.window, GA_PARENT);
                                RECT mapped = screen;
                                if (parent != HWND_DESKTOP) {
                                    MapWindowPoints(HWND_DESKTOP, parent, (LPPOINT)&mapped, 2);
                                }
                                Measurement m(
                                    XPoint(mapped.left),
                                    YPoint(mapped.top),
                                    XPoint(mapped.right),
                                    YPoint(mapped.bottom)
                                );
                                observer->OnNext(m);
                            }),
                        // on completed
                            [=](){
                                observer->OnCompleted();
                            }, 
                        // on error
                            [=](const std::exception_ptr& e){
                                observer->OnError(e);
                        });
                });
        }

    };

}

template<class Tag>
struct select_client_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_client_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectClientMeasurement(source)) {
    return      detail::operators<Tag>::SelectClientMeasurement(source);
}

template<class Tag>
struct select_screen_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_screen_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectScreenMeasurement(source)) {
    return      detail::operators<Tag>::SelectScreenMeasurement(source);
}

template<class Tag>
struct select_parent_measurement {};
template<class Tag, class Message>
auto rxcpp_chain(select_parent_measurement<Tag>&&, const std::shared_ptr<rx::Observable<Message>>& source) 
    -> decltype(detail::operators<Tag>::SelectParentMeasurement(source)) {
    return      detail::operators<Tag>::SelectParentMeasurement(source);
}

template<class Tag>
struct window_measure {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef typename traits::XPoint XPoint;
    typedef typename traits::YPoint YPoint;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef typename traits::Rect Rect;
    typedef typename traits::Measurement Measurement;

    typedef select_client_measurement<Tag> select_client_measurement;
    typedef select_screen_measurement<Tag> select_screen_measurement;
    typedef select_parent_measurement<Tag> select_parent_measurement;
};

template<class Tag>
struct Measurement {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef typename traits::XPoint XPoint;
    typedef typename traits::YPoint YPoint;
    typedef typename traits::XExtent XExtent;
    typedef typename traits::YExtent YExtent;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef typename traits::Rect Rect;

    Measurement() {}
#if 0
    explicit Measurement(Rect r) : 
        left(XPoint(r.r.left)),
        top(YPoint(r.r.top)),
        right(XPoint(r.r.right)),
        bottom(YPoint(r.r.bottom)) {}
#endif
    Measurement(
        XPoint l, YPoint t,
        XPoint r, YPoint b) : 
        left(std::move(l)), top(std::move(t)),
        right(std::move(r)), bottom(std::move(b)) {}
    XPoint left;
    YPoint top;
    XPoint right;
    YPoint bottom;
    XExtent width() const {return right - left;}
    YExtent height() const {return bottom - top;}
    XExtent width(float factor) const {return (right - left) * factor;}
    YExtent height(float factor) const {return (bottom - top) * factor;}
    Point origin() const {return Point(left, top);}
    Extent extent() const {return Extent(width(), height());}
    XPoint center() const {return left + (width() / 2);}
    YPoint middle() const {return top + (height() / 2);}
};

template<class Tag>
bool operator==(const Measurement<Tag>& l, const Measurement<Tag>& r) {
    return l.left == r.left && l.top == r.top && l.right == r.right && l.bottom == r.bottom;
}

}
using rxmsg::rxcpp_chain;

namespace rxmsg{namespace traits{
    template<class XPointArg>
    struct XPoint {
        XPointArg c;
        XPoint() {c = 0;}
        explicit XPoint(XPointArg carg) {c = carg;}
        XPoint(const XPoint& carg) : c(carg.c) {}
    };
    template<class YPointArg>
    struct YPoint {
        YPointArg c;
        YPoint() {c = 0;}
        explicit YPoint(YPointArg carg) {c = carg;}
        YPoint(const YPoint& carg) : c(carg.c) {}
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
    template<class XPointArg, class YPointArg, class PointArg>
    struct Point {
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        PointArg p;
        Point() {p.x = 0; p.y = 0;}
        Point(XPoint x, YPoint y) {p.x = x.c; p.y = y.c;}
        explicit Point(PointArg parg) {p = parg;}
        Point(const Point& p) : p(p.p) {}
        XPoint getX() {return XPoint(p.x);}
        YPoint getY() {return YPoint(p.y);}
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
        XExtent getCX() {return XExtent(e.cx);}
        YExtent getCY() {return YExtent(e.cy);}
    };
    template<class XPointArg, class YPointArg, class RectArg>
    struct Rect {
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        RectArg r;
        Rect() {r.left = 0; r.top = 0; r.right = 0; r.bottom = 0;}
        Rect(XPoint l, YPoint t, XPoint r, YPoint b) {
            r.left = l.c; r.top = t.c; r.right = r.c; r.bottom = b.c;}
        explicit Rect(RectArg rarg) {r = rarg;}
        Rect(const Rect& p) : p(p.p) {}
        XPoint getLeft() {return XPoint(r.left);}
        YPoint getTop() {return YPoint(r.top);}
        XPoint getRight() {return XPoint(r.right);}
        YPoint getBottom() {return YPoint(r.bottom);}
    };

    template<class XPointArg>
    auto operator-(XPoint<XPointArg> r, XPoint<XPointArg> l) -> XExtent<decltype(r.c - l.c)> {return XExtent<decltype(r.c - l.c)>(r.c - l.c);}
    template<class XPointArg>
    bool operator<(XPoint<XPointArg> r, XPoint<XPointArg> l) {return r.c < l.c;}
    template<class XPointArg>
    bool operator==(XPoint<XPointArg> r, XPoint<XPointArg> l) {return r.c == l.c;}

    template<class YPointArg>
    auto operator-(YPoint<YPointArg> b, YPoint<YPointArg> t) -> YExtent<decltype(b.c - t.c)> {return YExtent<decltype(b.c - t.c)>(b.c - t.c);}
    template<class YPointArg>
    bool operator<(YPoint<YPointArg> b, YPoint<YPointArg> t) {return b.c < t.c;}
    template<class YPointArg>
    bool operator==(YPoint<YPointArg> b, YPoint<YPointArg> t) {return b.c == t.c;}

    template<class XPointArg, class XExtentArg>
    auto operator-(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {return XPoint<XPointArg>(c.c - e.c);}
    template<class XPointArg, class XExtentArg>
    auto operator+(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {return XPoint<XPointArg>(c.c + e.c);}

    template<class YPointArg, class YExtentArg>
    auto operator-(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {return YPoint<YPointArg>(c.c - e.c);}
    template<class YPointArg, class YExtentArg>
    auto operator+(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {return YPoint<YPointArg>(c.c + e.c);}

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

    template<class XPointArg, class YPointArg, class PointArg>
    bool operator<(Point<XPointArg, YPointArg, PointArg> l, Point<XPointArg, YPointArg, PointArg> r) {
        return l.p.x < r.p.x || l.p.y < r.p.y;}
    template<class XPointArg, class YPointArg, class PointArg>
    bool operator==(Point<XPointArg, YPointArg, PointArg> l, Point<XPointArg, YPointArg, PointArg> r) {
        return l.p.x == r.p.x || l.p.y == r.p.y;}

    template<class Tag, class XPointArg, class YPointArg, class XExtentArg, class YExtentArg, class PointArg, class ExtentArg, class RectArg>
    struct measurement_traits_builder {
        typedef Tag tag;
        typedef XPointArg XPointRaw;
        typedef YPointArg YPointRaw;
        typedef XExtentArg XExtentRaw;
        typedef YExtentArg YExtentRaw;
        typedef PointArg PointRaw;
        typedef ExtentArg ExtentRaw;
        typedef RectArg RectRaw;
        typedef XPoint<XPointArg> XPoint;
        typedef YPoint<YPointArg> YPoint;
        typedef XExtent<XExtentArg> XExtent;
        typedef YExtent<YExtentArg> YExtent;
        typedef Point<XPointArg, YPointArg, PointArg> Point;
        typedef Extent<XExtentArg, YExtentArg, ExtentArg> Extent;
        typedef Rect<XPointArg, YPointArg, RectArg> Rect;
        typedef Measurement<tag> Measurement;
    };

    struct Default {};
    measurement_traits_builder<Default, int, int, int, int, POINT, SIZE, RECT> rx_measurement_traits(Default&&);
}}
using rxmsg::traits::rx_measurement_traits;

#if 0
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
#endif

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
        typedef rxmsg::window_measure<rxmsg::traits::Default> label_measure;
        typedef label_measure::XPoint XPoint;
        typedef label_measure::YPoint YPoint;
        typedef label_measure::XExtent XExtent;
        typedef label_measure::YExtent YExtent;
        typedef label_measure::Point Point;
        typedef std::shared_ptr<rx::BehaviorSubject<Point>> SubjectPoint;

        label(std::wstring text, POINT p, HWND parent, HINSTANCE hInstance)
            : text(std::move(text))
            , window(CreateChildWindow(L"Static", this->text.c_str(), p, hInstance, parent))
            , messages(rx::CreateSubject<rxmsg::subclass_message>())
            , subclass(rxmsg::set_window_subclass(window.get(), messages))
        {}
        label(label&& o) 
            : text(std::move(o.text))
            , window(std::move(o.window))
            , messages(std::move(o.messages))
            , subclass(std::move(o.subclass))
        {}
        label& operator=(label o) {
            using std::swap;
            swap(text,o.text);
            swap(window,o.window);
            swap(messages,o.messages);
            swap(subclass,o.subclass);
        }

        std::wstring text;
        l::wr::unique_destroy_window window;
        rxmsg::subclass_message::Subject messages;
        rxmsg::set_subclass_result subclass;
    };

    // note: no base class
    struct window
    {
        typedef rxmsg::window_measure<rxmsg::traits::Default> top_measure;
        window(HWND handle, CREATESTRUCT& cs)
            : root(handle)
            , exceptions(reinterpret_cast<Exceptions*>(cs.lpCreateParams))
            , text(rx::CreateSubject<std::wstring>())
            , messages(rx::CreateSubject<rxmsg::message>())
            , edit(CreateChildWindow(L"Edit", L"", POINT(), cs.hInstance, handle))
            , editMessages(rx::CreateSubject<rxmsg::subclass_message>())
            , editSubclass(rxmsg::set_window_subclass(edit.get(), editMessages))
            , mousePoint(rx::CreateBehaviorSubject<typename top_measure::Point>(top_measure::Point()))
        {
            auto worker = std::make_shared<rx::EventLoopScheduler>();
            auto mainFormScheduler = std::make_shared<rx::win32::WindowScheduler>();

            auto rootMeasurement = rx::from(messages)
                .chain<top_measure::select_client_measurement>();

            rx::from(rootMeasurement)
                .distinct_until_changed()
                // delay on worker thread
                .delay(std::chrono::milliseconds(100), worker)
                .observe_on(mainFormScheduler)
                .subscribe(
                    [this](top_measure::Measurement m){
                        SetWindowPos(edit.get(), nullptr, m.left.c, m.top.c, m.width().c, 30, SWP_NOOWNERZORDER);
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
                    POINT p = {x, y}; mousePoint->OnNext(top_measure::Point(p));}));

            rx::from(text)
                .subscribe(
                // on next
                    [=](const std::wstring& msg){
                        // set up labels and query

                        cd.Dispose();
                        labels.clear();

                        auto point = rx::from(mousePoint)
                            // make the text appear above the mouse location
                            .select([](top_measure::Point p){p.p.y -= 30; return p;});

                        for (int i = 0; msg[i]; ++i)
                        {
                            POINT loc = {20 * i, 30};
                            auto& charlabel = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);

                            rx::from(point)
                                .distinct_until_changed()
                                // delay on worker thread
                                .delay(std::chrono::milliseconds(100), worker)
                                .observe_on(mainFormScheduler)
                                .subscribe([&charlabel](top_measure::Point p){
                                    SetWindowPos(charlabel.window.get(), nullptr, p.p.x, p.p.y, 20, 30, SWP_NOOWNERZORDER);
                                    InvalidateRect(charlabel.window.get(), nullptr, true);
                                });

                            point = rx::from(charlabel.messages)
                                .chain<top_measure::select_parent_measurement>()
                                // put the next char at the right - top of this char
                                .select([](top_measure::Measurement m){return top_measure::Point(m.right, m.top);});
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
        l::wr::unique_destroy_window edit;
        rxmsg::subclass_message::Subject editMessages;
        rxmsg::set_subclass_result editSubclass;
        rx::ComposableDisposable cd;
        std::list<label> labels;
        label::SubjectPoint mousePoint;
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

