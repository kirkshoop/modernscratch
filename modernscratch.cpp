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
#include <Uxtheme.h>
#include <vsstyle.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "uxtheme.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

#include <cmath>
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
// this 'too clever for its own good' class
// will use the comma operator to reuse
// the windowsx.h HANDLE_WM_ macros 
struct extract_lresult {
    LRESULT operator,(LRESULT lResult) {
        return lResult;}
    operator LRESULT() {return 0;}
};
}

template<class Tag, unsigned int Id, class... T>
struct message_trait_builder {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<T..., Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND, T... t) -> detail::extract_lresult {
            result = args_type(t..., m); return detail::extract_lresult();}));
        return result;
    }
};

template<class Tag>
struct message_traits {
    typedef decltype(rxmsg_traits(Tag())) type;
};

template<class Tag>
struct messageId {
    typedef typename message_traits<Tag>::type message_traits;
    static const unsigned int id = message_traits::id; 

    template<class Msg>
    bool operator()(const Msg& m) const {
        return !handled(m) && m.id == id;
    }
};

template<class Tag, class Message>
auto handle_message(Message m) 
    -> decltype(message_traits<Tag>::type::crack(m)) {
    set_handled(m);
    return message_traits<Tag>::type::crack(m);
}

template<class Tag, class Message>
auto crack_message(Message m) 
    -> decltype(message_traits<Tag>::type::crack(m)) {
    return message_traits<Tag>::type::crack(m);
}

namespace wm {
    namespace detail {
    struct paint {};
    message_trait_builder<paint, WM_PAINT> rxmsg_traits(paint&&);
    template<class M, class F>
    LRESULT rxmsg_crack(paint&&, M message, F function){
        return HANDLE_WM_PAINT(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::paint paint;

    namespace detail {
    struct printclient {};
    message_trait_builder<printclient, WM_PRINTCLIENT, HDC, UINT> rxmsg_traits(printclient&&);
    template<class M, class F>
    LRESULT rxmsg_crack(printclient&&, M message, F function){
        return function(message.window, reinterpret_cast<HDC>(message.wParam), static_cast<UINT>(message.lParam));}
    }
    typedef detail::printclient printclient;

    namespace detail {
    struct windowposchanged {};
    message_trait_builder<windowposchanged, WM_WINDOWPOSCHANGED, LPWINDOWPOS> rxmsg_traits(windowposchanged&&);
    template<class M, class F>
    LRESULT rxmsg_crack(windowposchanged&&, M message, F function){
        return HANDLE_WM_WINDOWPOSCHANGED(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::windowposchanged windowposchanged;

    namespace detail {
    struct erasebkgnd {};
    message_trait_builder<erasebkgnd, WM_ERASEBKGND, HDC> rxmsg_traits(erasebkgnd&&);
    template<class M, class F>
    LRESULT rxmsg_crack(erasebkgnd&&, M message, F function){
        return HANDLE_WM_ERASEBKGND(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::erasebkgnd erasebkgnd;

    namespace detail {
    struct command {};
    message_trait_builder<command, WM_COMMAND, int, HWND, UINT> rxmsg_traits(command&&);
    template<class M, class F>
    LRESULT rxmsg_crack(command&&, M message, F function){
        return HANDLE_WM_COMMAND(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::command command;

    namespace detail {
    struct ncdestroy {};
    message_trait_builder<ncdestroy, WM_NCDESTROY> rxmsg_traits(ncdestroy&&);
    template<class M, class F>
    LRESULT rxmsg_crack(ncdestroy&&, M message, F function){
        return HANDLE_WM_NCDESTROY(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::ncdestroy ncdestroy;

    namespace detail {
    struct lbuttondown {};
    message_trait_builder<lbuttondown, WM_LBUTTONDOWN, BOOL, int, int, UINT> rxmsg_traits(lbuttondown&&);
    template<class M, class F>
    LRESULT rxmsg_crack(lbuttondown&&, M message, F function){
        return HANDLE_WM_LBUTTONDOWN(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::lbuttondown lbuttondown;

    namespace detail {
    struct lbuttonup {};
    message_trait_builder<lbuttonup, WM_LBUTTONUP, int, int, UINT> rxmsg_traits(lbuttonup&&);
    template<class M, class F>
    LRESULT rxmsg_crack(lbuttonup&&, M message, F function){
        return HANDLE_WM_LBUTTONUP(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::lbuttonup lbuttonup;

    namespace detail {
    struct mousemove {};
    message_trait_builder<mousemove, WM_MOUSEMOVE, int, int, UINT> rxmsg_traits(mousemove&&);
    template<class M, class F>
    LRESULT rxmsg_crack(mousemove&&, M message, F function){
        return HANDLE_WM_MOUSEMOVE(message.window, message.wParam, message.lParam, function);}
    }
    typedef detail::mousemove mousemove;

    namespace detail {
    struct themechanged {};
    message_trait_builder<themechanged, WM_THEMECHANGED> rxmsg_traits(themechanged&&);
    }
    typedef detail::themechanged themechanged;
}

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
                        .where(messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
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
                        .where(messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
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
                        .where(messageId<rxmsg::wm::windowposchanged>())
                        .select([](const Message& msg){
                            return rxmsg::crack_message<rxmsg::wm::windowposchanged>(msg);})
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
using rxmsg::wm::detail::rxmsg_traits;
using rxmsg::wm::detail::rxmsg_crack;

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
    auto operator-(XPoint<XPointArg> r, XPoint<XPointArg> l) -> XExtent<decltype(r.c - l.c)> {
        return XExtent<decltype(r.c - l.c)>(r.c - l.c);}
    template<class XPointArg>
    bool operator<(XPoint<XPointArg> r, XPoint<XPointArg> l) {
        return r.c < l.c;}
    template<class XPointArg>
    bool operator==(XPoint<XPointArg> r, XPoint<XPointArg> l) {
        return r.c == l.c;}

    template<class YPointArg>
    auto operator-(YPoint<YPointArg> b, YPoint<YPointArg> t) -> YExtent<decltype(b.c - t.c)> {
        return YExtent<decltype(b.c - t.c)>(b.c - t.c);}
    template<class YPointArg>
    bool operator<(YPoint<YPointArg> b, YPoint<YPointArg> t) {
        return b.c < t.c;}
    template<class YPointArg>
    bool operator==(YPoint<YPointArg> b, YPoint<YPointArg> t) {
        return b.c == t.c;}

    template<class XPointArg, class XExtentArg>
    auto operator-(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {
        return XPoint<XPointArg>(c.c - e.c);}
    template<class XPointArg, class XExtentArg>
    auto operator+(XPoint<XPointArg> c, XExtent<XExtentArg> e) -> XPoint<XPointArg> {
        return XPoint<XPointArg>(c.c + e.c);}

    template<class YPointArg, class YExtentArg>
    auto operator-(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {
        return YPoint<YPointArg>(c.c - e.c);}
    template<class YPointArg, class YExtentArg>
    auto operator+(YPoint<YPointArg> c, YExtent<YExtentArg> e) -> YPoint<YPointArg> {
        return YPoint<YPointArg>(c.c + e.c);}

    template<class XExtentArg>
    bool operator<(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {
        return l.c < r.c;}
    template<class XExtentArg>
    bool operator==(XExtent<XExtentArg> l, XExtent<XExtentArg> r) {
        return l.c == r.c;}

    template<class YExtentArg>
    bool operator<(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {
        return l.c < r.c;}
    template<class YExtentArg>
    bool operator==(YExtent<YExtentArg> l, YExtent<YExtentArg> r) {
        return l.c == r.c;}

    template<class XExtentArg>
    auto operator/(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {
        return XExtent<XExtentArg>(e.c/d);}
    template<class XExtentArg>
    auto operator*(XExtent<XExtentArg> e, XExtentArg d) -> XExtent<XExtentArg> {
        return XExtent<XExtentArg>(e.c*d);}

    template<class YExtentArg>
    auto operator/(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {
        return YExtent<YExtentArg>(e.c/d);}
    template<class YExtentArg>
    auto operator*(YExtent<YExtentArg> e, YExtentArg d) -> YExtent<YExtentArg> {
        return YExtent<YExtentArg>(e.c*d);}

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

namespace rxtheme {

template<class MessageArg>
class theme_data {
public:
    typedef MessageArg Message;

    ~theme_data() {
        themechanged.Dispose();}

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
        };

        themechanged.Set(rx::from(messages)
            .where(rxmsg::messageId<rxmsg::wm::themechanged>())
            .subscribe([loadThemeData](const Message& m){
                rxmsg::set_handled(m); loadThemeData();}));

        loadThemeData();
    }

    HWND window;
    rx::SerialDisposable themechanged;
    std::wstring classId;
    typename Message::Observable messages;
    l::wr::unique_theme_data handle;

private:
};

template<class Tag, class MessageArg>
struct label {
    typedef Tag tag;
    typedef decltype(rx_measurement_traits(tag())) traits;
    typedef MessageArg Message;
    typedef typename traits::Rect Rect;
    typedef typename traits::Measurement Measurement;

    label(HWND w, const typename Message::Observable& m) : 
        window(w), messages(m), theme(std::make_shared<theme_data<Message>>(window, L"static", messages)) {
    } 
    HWND window;
    typename Message::Observable messages;
    std::shared_ptr<theme_data<Message>> theme;
    Measurement measureText() 
    {
        Rect r;
        Measurement m;
        HDC dc = GetDC(window);
        auto text = l::wnd::get_text(window);
        GetThemeTextExtent(
          theme->handle.get(),
          dc,
          TEXT_LABEL,
          0,
          text.c_str(),
          text.size(),
          DT_LEFT | DT_TOP,
          nullptr,
          &r.r
        );
        m.left = r.getLeft();
        m.top = r.getTop();
        m.right = r.getRight();
        m.bottom = r.getBottom();
        return m;
    }
};

}

namespace rxanim {
    template<class Clock>
    class time_range {
    public:
        typedef Clock clock;
        typedef typename clock::time_point time_point;
        typedef typename clock::duration duration_type;

        time_range() {}
        time_range(time_point s, time_point f) : start(s), finish(f) {}
        time_range(time_point s, duration_type d) : start(s), finish(s + d) {}
        time_point start;
        time_point finish;
        bool empty() {return start == finish;}
        bool contains(time_point p) {return p >= start && p < finish;}
        duration_type duration() {return finish - start;}
    };


    float easeNone(float t) {return t;}
    float easeSquare(float t) {return t*t;}
    float easeSquareRoot(float t) {return sqrt(t);}
    template<int Num, int Den = 1>
    float easePow(float t) {return pow(t, Num/Den);}
    template<int Den>
    float easeRoot(float t) {return pow(t, 1/Den);}
    float easeElasticDamp(float t) {
        return 1.0f - pow(2, -10 * t) * sin(4 * 3.14 * (t+3.14));}
    float easeElasticAmp(float t) {
        return pow(2, 10 * (t - 1)) * sin(4 * 3.14 * (t+3.14));}
    float easeBounceDamp(float t) {
        if (t < .27) {
            return (4*3.14*t*t);}
        else if (t < .71) {
            return (4*3.14*(t-.5)*(t-.5))+.4;}
        else if (t < .89) {
            return (4*3.14*(t-.8)*(t-.8))+.92;}
        else {
            return (4*3.14*(t-.94)*(t-.94))+.97;}}
    float easeBounceAmp(float t) {
        return 1.0f - easeBounceDamp(t);}

    struct animation_state {
        enum type {Invalid, Pending, Running, Finished};
    };

    template<class TimeRange, class TimePoint>
    animation_state::type runOnce(TimeRange r, TimePoint p) {
        if (p < r.start) {
            return animation_state::Pending;}
        if (r.empty() || !r.contains(p)) {
            return animation_state::Finished;}
        return animation_state::Running;
    }

    template<size_t N, class TimeRange, class TimePoint>
    animation_state::type runN(TimeRange r, TimePoint p) {
        TimeRange full = r;
        full.finish = r.start + (r.duration() * N);
        return runOnce(full, p);
    }

    template<class TimeRange, class TimePoint>
    float adjustNone(TimeRange r, TimePoint p) {
        size_t den = r.duration().count();
        size_t num = (p - r.start).count();
#if 0
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(p);
        logmsg << L"adjust  time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.start);
        logmsg << L"       start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.finish);
        logmsg << L"      finish: " << std::ctime(&tt); 
        logmsg << L"   numerator: " << num << std::endl; 
        logmsg << L" denominator: " << den << std::endl; 
        logmsg << L"       place: " << (num%den) << std::endl; 
        logmsg << L"   iteration: " << (num/den) << std::endl; 
        logmsg << L"  normalized: " << (static_cast<float>(num%den)/den) << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return static_cast<float>(num%den) / den;}

    template<class TimeRange, class TimePoint>
    float adjustReverse(TimeRange r, TimePoint p) {
        return 1.0f - adjustNone(r, p);}

    template<class TimeRange, class TimePoint>
    float adjustPingPong(TimeRange r, TimePoint p) {
        size_t den = r.duration().count();
        size_t num = (p - r.start).count();
        float none = adjustNone(r, p);
#if 0
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(p);
        logmsg << L"pingpong time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.start);
        logmsg << L"        start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(r.finish);
        logmsg << L"       finish: " << std::ctime(&tt);
        logmsg << L"    numerator: " << num << std::endl; 
        logmsg << L"  denominator: " << den << std::endl; 
        logmsg << L"        place: " << (num%den) << std::endl; 
        logmsg << L"    iteration: " << (num/den) << std::endl; 
        logmsg << L"   normalized: " << (static_cast<float>(num%den)/den) << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return 0==(static_cast<size_t>(num/den) % 2) ? none : 1.0f - none;}

    // used to implement bouncing etc..
    // in, is a percentage that represents the point in the time_range 
    typedef std::function<float (float)> ease_type;
    // called to execute the animation step at time_point as if it was at normalized time factor float
    typedef std::function<void (animation_state::type, float)> step_type;

    template<class Clock>
    class time_animation {
    public:
        typedef Clock clock;
        typedef typename clock::duration duration_type;
        typedef typename clock::time_point time_point;
        typedef time_range<clock> time_range;

        // used to constrain the run time of the animation
        typedef std::function<animation_state::type (time_range, time_point)> state_type;
        // used to implement ping/pong, repeat, etc..
        // returns a float from 0-1 that represents where in time_range  
        // the passed in time_point (may not be in time_range) represents.
        typedef std::function<float (time_range, time_point)> adjust_type;
        typedef ease_type ease_type;
        typedef step_type step_type;

        time_animation(duration_type u) : 
            update(u), state(runOnce<time_range, time_point>), adjust(adjustNone<time_range, time_point>), ease(easeNone)
            {}
        time_animation(duration_type u, state_type st) : 
            update(u), state(std::move(st)), adjust(adjustNone<time_range, time_point>), ease(easeNone) 
            {}
        time_animation(duration_type u, state_type st, ease_type ease) : 
            update(u), state(std::move(st)), adjust(adjustNone<time_range, time_point>), ease(std::move(ease)) 
            {}
        time_animation(duration_type u, state_type st, adjust_type a) : 
            update(u), state(std::move(st)), adjust(std::move(a)), ease(easeNone)
            {}
        time_animation(duration_type u, state_type st, adjust_type a, ease_type e) : 
            update(u), state(std::move(st)), adjust(std::move(a)), ease(std::move(e)) 
            {}

        duration_type update;
        state_type state;
        adjust_type adjust;
        ease_type ease;
    };

    template<class Clock>
    animation_state::type step(time_animation<Clock>& ta, time_range<Clock> sc, time_range<Clock> sc_st, step_type& st, typename Clock::time_point t) {
        auto state = ta.state(sc, t);
        if (state == animation_state::Pending) {
            return state;}
        float time = 0.0f;
        if (state == animation_state::Running) {
            auto sc_time = ta.ease(ta.adjust(sc, t));
            auto sc_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * sc_time);
            if (!sc_st.contains(sc_timepoint)) {return animation_state::Pending;}
            time = (sc_timepoint - sc_st.start).count() / (sc_st.finish - sc_st.start).count();}
        else if (state == animation_state::Finished) {
            time = 1.0f;}
        st(state, time);
#if 0
        size_t den = sc.duration().count();
        size_t num = (t - sc.start).count();
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        logmsg << L"step      time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.start);
        logmsg << L"         start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.finish);
        logmsg << L"        finish: " << std::ctime(&tt); 
        logmsg << L"         state: " << (state == animation_state::Pending ? "Pending" : 
                                       (state == animation_state::Running ? "Running" : 
                                       (state == animation_state::Finished ? "Finished" : "Unknown"))) << std::endl; 
        logmsg << L"     numerator: " << num << std::endl; 
        logmsg << L"   denominator: " << den << std::endl; 
        if (den != 0) {
            logmsg << L"         place: " << (num%den) << std::endl; 
            logmsg << L"     iteration: " << (num/den) << std::endl; 
            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
        logmsg << L"        result: " << time << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return state;
    }

    template<class Clock>
    animation_state::type step(time_animation<Clock>& ta, time_range<Clock> sc, time_range<Clock> sc_st, step_type& st) {
        auto t = Clock::now();
        if (!sc_st.contains(t)) {return animation_state::Pending;}
        auto state = ta.state(sc, t);
        if (state == animation_state::Pending) {
            return state;}
        float time = 0.0f;
        if (state == animation_state::Running) {
            auto sc_time = ta.ease(ta.adjust(sc, t));
            auto sc_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * sc_time);
            if (!sc_st.contains(sc_timepoint)) {return animation_state::Pending;}
            time = (sc_timepoint - sc_st.start).count() / (sc_st.finish - sc_st.start).count();}
        else if (state == animation_state::Finished) {
            time = 1.0f;}
        st(state, time);
#if 0
        size_t den = sc.duration().count();
        size_t num = (t - sc.start).count();
        std::wstringstream logmsg;
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        logmsg << L"step(now) time: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.start);
        logmsg << L"         start: " << std::ctime(&tt); 
        tt = std::chrono::system_clock::to_time_t(sc.finish);
        logmsg << L"        finish: " << std::ctime(&tt); 
        logmsg << L"         state: " << (state == animation_state::Pending ? "Pending" : 
                                       (state == animation_state::Running ? "Running" : 
                                       (state == animation_state::Finished ? "Finished" : "Unknown"))) << std::endl; 
        logmsg << L"     numerator: " << num << std::endl; 
        logmsg << L"   denominator: " << den << std::endl; 
        if (den != 0) {
            logmsg << L"         place: " << (num%den) << std::endl; 
            logmsg << L"     iteration: " << (num/den) << std::endl; 
            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
        logmsg << L"        result: " << time << std::endl; 
        OutputDebugString(logmsg.str().c_str());
#endif
        return state;
    }

    namespace detail {
    template<class T, class Indices>
    struct tuple_insert;
    template<class T, size_t... DisptachIndices>
    struct tuple_insert<T, rx::util::tuple_indices<DisptachIndices...>> {
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
        -> decltype(detail::tuple_insert<T, typename rx::util::make_tuple_indices<T>::type>(t)) {
        return      detail::tuple_insert<T, typename rx::util::make_tuple_indices<T>::type>(t);
    }


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

    template<class T>
    struct lerp_value {
        lerp_value(std::shared_ptr<rx::Observer<T>> o, T i, T f) : initial(std::move(i)), final(std::move(f)), observer(std::move(o)) {}
        T initial;
        T final;
        std::shared_ptr<rx::Observer<T>> observer;
        void operator()(animation_state::type state, float time) {
            observer->OnNext(initial + ((final - initial) * time));
            if (state == animation_state::Finished) {
                observer->OnCompleted();}
        }
    };

    template<class... Value>
    struct lerp_value<std::tuple<Value...>> {
        typedef std::tuple<Value...> T;
        lerp_value(std::shared_ptr<rx::Observer<T>> o, T i, T f) : initial(std::move(i)), final(std::move(f)), observer(std::move(o)) {}
        lerp_value(std::shared_ptr<rx::Observer<T>> o, Value... i, Value... f)
            : inital(std::make_tuple(std::move(i)...)), final(std::make_tuple(std::move(f)...)), observer(std::move(o)) {}
        T initial;
        T final;
        std::shared_ptr<rx::Observer<T>> observer;
        void operator()(animation_state::type state, float time) {
            rx::DispatchTuple(std::tuple_cat(initial, final), [time, this](Value... initialv, Value... finalv) {
                T result(static_cast<Value>(initialv + ((finalv - initialv) * time))...);
#if 0
                std::wstringstream logmsg;
                logmsg << L"step - initial: " << tuple_insert(initial) << std::endl; 
                logmsg << L"step -   final: " << tuple_insert(final) << std::endl; 
                logmsg << L"step -  result: " << tuple_insert(result) << std::endl; 
                OutputDebugString(logmsg.str().c_str());
#endif
                observer->OnNext(std::move(result));
            });
        }
    };

    template<class T, class TimeSelector>
    auto Animate(
        const std::shared_ptr<rx::Observable<T>>& sourceFinal, 
        const std::shared_ptr<rx::Observable<typename rx::Scheduler::clock::time_point>>& sourceInterval, 
        time_animation<typename rx::Scheduler::clock> ta,
        TimeSelector timeSelector 
        ) 
        -> std::shared_ptr<rx::Observable<T>> {
        typedef typename rx::Scheduler::clock clock;
        typedef time_animation<clock> time_animation;
        typedef typename time_animation::time_range time_range;
        typedef typename time_animation::time_point time_point;
        typedef typename time_animation::duration_type time_duration;
        typedef typename time_animation::step_type step_type;
        typedef std::pair<time_range, lerp_value<T>> lerp_value_type;
        typedef std::vector<std::shared_ptr<lerp_value_type>> lerps_type;
        typedef typename lerps_type::difference_type difference_type;
        return rx::CreateObservable<T>(
        // subscribe
            [=](const std::shared_ptr<rx::Observer<T>>& observer) 
                -> rx::Disposable {
                struct State {
                    State(time_animation ta) : 
                        animation(std::move(ta)) {}
                    std::mutex lock;
                    lerps_type lerps;
                    time_animation animation;
                };
                auto state = std::make_shared<State>(ta);
                rx::ComposableDisposable cd;
#if 1
                cd.Add(rx::from(sourceFinal)
                    .subscribe(
                    // on next
                        [=](T final) {
                            auto now = clock::now();
                            auto finish = timeSelector(now, final);

                            std::unique_lock<std::mutex> guard(state->lock);

                            auto start = now;
                            auto initial = final;

                            if (!state->lerps.empty()) {
                                start = state->lerps.back()->first.finish;
                                initial = state->lerps.back()->second.final;}

                            auto scope = time_range(start, finish);

                            state->lerps.push_back(std::make_shared<lerp_value_type>(
                                scope, 
                                lerp_value<T>(observer, initial, final)));
                        },
                    // on completed
                        [=](){
                            observer->OnCompleted();
                            cd.Dispose();
                        }, 
                    // on error
                        [=](const std::exception_ptr& e){
                            observer->OnError(e);
                            cd.Dispose();
                    }));

                cd.Add(rx::from(sourceInterval)
                    .subscribe(
                    // on next
                        [=](time_point thisTick) {
#if 0
                            {std::wstringstream logmsg;
                            std::time_t tt = std::chrono::system_clock::to_time_t(thisTick);
                            logmsg << L"Tick: " << std::ctime(&tt) << std::endl; 
                            OutputDebugString(logmsg.str().c_str());}
#endif
                            lerps_type lerps;
                            {
                                std::unique_lock<std::mutex> guard(state->lock);
                                lerps = state->lerps;
                            }

                            if (!lerps.empty()) {
                                auto begin = lerps.begin();
                                auto cursor = lerps.begin();
                                auto end = lerps.end();
                                auto sc = lerps.front()->first;
                                sc.finish = lerps.back()->first.finish;
                                auto sc_state = ta.state(sc, thisTick);
                                if (sc_state == animation_state::Running) {
                                    auto sc = lerps.front()->first;
                                    sc.finish = lerps.back()->first.finish;

                                    auto st_time = state->animation.ease(state->animation.adjust(sc, thisTick));

                                    auto st_timepoint = sc.start + std::chrono::duration_cast<std::chrono::milliseconds>(sc.duration() * st_time);
                                    for (;cursor != end && !(*cursor)->first.contains(st_timepoint); ++cursor);

                                    if (cursor != end) {
                                        auto& lerp = *cursor;
                                        auto time = static_cast<float>((st_timepoint - lerp->first.start).count()) / (lerp->first.finish - lerp->first.start).count();
#if 0
                                        size_t num = (st_timepoint - lerp->first.start).count();
                                        size_t den = (lerp->first.finish - lerp->first.start).count();
                                        std::wstringstream logmsg;
                                        logmsg <<     L"tick      time: " << time_insert(thisTick) << std::endl; 
                                        logmsg <<     L"    eased time: " << time_insert(st_timepoint) << std::endl; 
                                        logmsg <<     L"         scope: " << time_insert(sc.start) << L"-" << time_insert(sc.finish) << std::endl; 
                                        logmsg <<     L"    step scope: " << time_insert(lerp->first.start) << L"-" << time_insert(lerp->first.finish) << std::endl; 
                                        logmsg <<     L"     numerator: " << num << std::endl; 
                                        logmsg <<     L"   denominator: " << den << std::endl; 
                                        if (den != 0) {
                                            logmsg << L"         place: " << (num%den) << std::endl; 
                                            logmsg << L"     iteration: " << (num/den) << std::endl; 
                                            logmsg << L"    normalized: " << (static_cast<float>(num%den)/den) << std::endl;}
                                        logmsg <<     L"        result: " << time << std::endl << std::endl; 
                                        OutputDebugString(logmsg.str().c_str());
#endif
                                        lerp->second(sc_state, time);}}
                                else if (sc_state == animation_state::Finished) {
                                    lerps.back()->second(sc_state, 1.0f);}

                                {
                                    std::unique_lock<std::mutex> guard(state->lock);
                                    if (sc_state == animation_state::Finished) {
                                        state->lerps.clear();}
                                }
                            }
                        },
                    // on completed
                        [=](){
                            observer->OnCompleted();
                            cd.Dispose();
                        }, 
                    // on error
                        [=](const std::exception_ptr& e){
                            observer->OnError(e);
                            cd.Dispose();
                    }));
#endif
                return cd;
            });
    }

    struct animate {};
    template<class T, class TimeSelector>
    auto rxcpp_chain(animate&&, 
        const std::shared_ptr<rx::Observable<T>>& sourceFinal, 
        const std::shared_ptr<rx::Observable<typename rx::Scheduler::clock::time_point>>& sourceInterval, 
        time_animation<typename rx::Scheduler::clock> ta,
        TimeSelector timeSelector 
        ) 
        -> decltype(Animate(sourceFinal, sourceInterval, ta, timeSelector)) {
        return      Animate(sourceFinal, sourceInterval, ta, timeSelector);
    }
}
using rxanim::rxcpp_chain;

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
        typedef rxtheme::label<rxmsg::traits::Default, rxmsg::subclass_message> text_measure;
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
            , textmeasure(window.get(), messages)
        {}
        label(label&& o) 
            : text(std::move(o.text))
            , window(std::move(o.window))
            , messages(std::move(o.messages))
            , subclass(std::move(o.subclass))
            , textmeasure(std::move(o.textmeasure))
        {}
        label& operator=(label o) {
            using std::swap;
            swap(text,o.text);
            swap(window,o.window);
            swap(messages,o.messages);
            swap(subclass,o.subclass);
            swap(textmeasure, o.textmeasure);
        }

        std::wstring text;
        l::wr::unique_destroy_window window;
        rxmsg::subclass_message::Subject messages;
        rxmsg::set_subclass_result subclass;
        text_measure textmeasure;
    };

    struct LabelCounts {
        std::wstring text;
        Count source;
        Count distinct;
        Count animated;
        Count observed;
    };

    // note: no base class
    struct window
    {
        typedef rxmsg::window_measure<rxmsg::traits::Default> top_measure;
        typedef rx::Scheduler::clock clock;
        typedef rxanim::time_animation<clock> time_animation;
        typedef time_animation::adjust_type adjust_type;
        typedef time_animation::state_type state_type;
        typedef time_animation::time_range time_range;
        typedef time_animation::time_point time_point;
        typedef time_animation::duration_type time_duration;

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
            auto mainFormScheduler = std::make_shared<rx::win32::WindowScheduler>();
            auto worker = std::make_shared<rx::EventLoopScheduler>();
            auto animateSource = std::make_shared<rx::EventLoopScheduler>();
            auto updateInterval = rx::from(rxcpp::Interval(std::chrono::milliseconds(30), animateSource))
                .select([](size_t ){return clock::now();})
                .publish();

            {
            rx::Disposable d([](){
                OutputDebugString(L"disposable::dispose\n");});
            d.Dispose();

            rx::SerialDisposable sd;
            sd.Set(rx::Disposable([](){
                OutputDebugString(L"SerialDisposable::dispose\n");}));
            sd.Dispose();

            rx::ComposableDisposable cd;
            for (int i = 0; i < 2; ++i) {
                cd.Dispose();
                cd.Add(rx::Disposable([](){
                    OutputDebugString(L"composabledisposible::disposable::dispose\n");}));
                cd.Add(sd);
                sd.Set(rx::Disposable([](){
                    OutputDebugString(L"composabledisposible::SerialDisposable::dispose\n");}));

                cd.Add([]() -> rx::Disposable {
                    rx::ComposableDisposable cd;
                    cd.Add(rx::Disposable([](){
                        OutputDebugString(L"nested composabledisposible::SerialDisposable::dispose\n");}));
                    return cd;
                }());
            }

            cd.Dispose();

            Count c;
            rx::SerialDisposable test;
            test.Set(rx::from(messages)
                .chain<record>(&c)
                .subscribe([=](const rxmsg::message&){
                    test.Dispose();}
            ));
            }

            auto rootMeasurement = rx::from(messages)
                .chain<top_measure::select_client_measurement>()
                .distinct_until_changed()
                .publish();

            auto editBounds = rx::from(rootMeasurement)
                .select([](top_measure::Measurement m){
                    return std::make_tuple(m.left.c, m.top.c, m.width().c);})
                .publish();

            auto ta = time_animation(
                std::chrono::milliseconds(30),
                state_type(rxanim::runN<3, time_range, time_point>), 
                adjust_type(rxanim::adjustPingPong<time_range, time_point>),
                rxanim::ease_type(rxanim::easeSquareRoot));

            rx::from(editBounds)
                // set the initial value
                .take(1)
                // merge the values to animate to
                .merge(editBounds)
                .chain<rxanim::animate>(updateInterval, ta,
                    [](time_point now, const std::tuple<int, int, int>&){
                        return now + std::chrono::milliseconds(500);})
                .observe_on(mainFormScheduler)
                .subscribe(
                    rx::MakeTupleDispatch([this](int left, int top, int width){
                        SetWindowPos(edit.get(), nullptr, left, top, width, 30, SWP_NOOWNERZORDER);
                        InvalidateRect(edit.get(), nullptr, true);
                        UpdateWindow(edit.get());
                    }));

            auto commands = rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::command>())
                .select([](const rxmsg::message& msg){
                    return rxmsg::handle_message<rxmsg::wm::command>(msg);})
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
                .where(rxmsg::messageId<rxmsg::wm::ncdestroy>())
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    cd->Dispose();
                    PostQuitMessage(0);
                });

            // print client
            rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::printclient>())
                .select([](const rxmsg::message& msg){
                    return rxmsg::handle_message<rxmsg::wm::printclient>(msg);})
                .subscribe(
                //on next
                    rxcpp::MakeTupleDispatch([this](HDC hdc, UINT, rxmsg::message m){
                        PAINTSTRUCT ps = {};
                        ps.hdc = hdc;
                        GetClientRect(m.window, &ps.rcPaint);
                        rxmsg::set_lResult(m, this->PaintContent(ps));
                    }),
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in printclient: ", e));});

            // paint
            rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::paint>())
                .subscribe(
                // on next
                    [this](const rxmsg::message& m){
                        set_handled(m);
                        PAINTSTRUCT ps = {};
                        BeginPaint(m.window, &ps);
                        l::wr::unique_gdi_end_paint ender(std::make_pair(m.window, &ps));
                        rxmsg::set_lResult(m, this->PaintContent(ps));
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in paint: ", e));});

            // disable erase background
            rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::erasebkgnd>())
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    set_lResult(m,TRUE);
                });

            auto mouseDown = rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::lbuttondown>())
                .publish();

            auto mouseUp = rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::lbuttonup>())
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
                        .where(rxmsg::messageId<rxmsg::wm::mousemove>())
                        .select([](const rxmsg::message& msg){
                            return rxmsg::handle_message<rxmsg::wm::mousemove>(msg);})
                        .distinct_until_changed()
                        .publish();})
                .subscribe(rxcpp::MakeTupleDispatch([=](int x, int y, UINT, const rxmsg::message&) {
                    POINT p = {x, y}; mousePoint->OnNext(top_measure::Point(p));}));

            rx::from(text)
                .subscribe(
                // on next
                    [=](const std::wstring& msg){
                        // set up labels and query

                        if (cd) {
                            cd->Dispose();
                        }
                        cd = std::make_shared<rx::ComposableDisposable>();
                        labels.clear();
#if 1
                        std::wstringstream logmsg;
                        for (auto& c : counts) {
                            logmsg << "'" << c.text << "'" << std::endl;
                            logmsg << " location: nexts, completions, errors, disposals" << std::endl;
                            logmsg << "   source: " << c.source.nexts   << ", " << c.source.completions   << ", " << c.source.errors   << ", " << c.source.disposals   << std::endl;
                            logmsg << " distinct: " << c.distinct.nexts << ", " << c.distinct.completions << ", " << c.distinct.errors << ", " << c.distinct.disposals << std::endl;
                            logmsg << " animated: " << c.animated.nexts << ", " << c.animated.completions << ", " << c.animated.errors << ", " << c.animated.disposals << std::endl;
                            logmsg << " observed: " << c.observed.nexts << ", " << c.observed.completions << ", " << c.observed.errors << ", " << c.observed.disposals << std::endl;
                        }
                        OutputDebugString(logmsg.str().c_str());
#endif
                        counts.clear();
                        maxHeight = 0;
                        int relativeX = 0;

                        auto point = rx::from(mousePoint)
                            // make the text appear above the mouse location
                            .select([this](top_measure::Point p){
                                p.p.y -= maxHeight; return std::tuple<int, int>(p.p.x, p.p.y);})
                            .publish();

                        for (int i = 0; msg[i]; ++i)
                        {
                            POINT loc = {20 * i, 30};
                            auto& charlabel = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);
                            auto labelMeasurement = charlabel.textmeasure.measureText();
                            counts.emplace_back();
                            auto& c = counts.back();
                            c.text.append(msg.begin() + i, msg.begin() + i + 1);

                            maxHeight = std::max(maxHeight, labelMeasurement.height().c);

                            cd->Add(rx::from(point)
                                .chain<record>(&c.source)
                                .distinct_until_changed()
                                .chain<record>(&c.distinct)
                                .chain<rxanim::animate>(
                                    updateInterval,
                                    ta,
                                    [=](time_point now, const std::tuple<int, int>&){
                                        return now + std::chrono::milliseconds(100 * i);})
                                .chain<record>(&c.animated)
                                .observe_on(mainFormScheduler)
                                .chain<record>(&c.observed)
                                .subscribe(
                                // on next
                                    rx::MakeTupleDispatch(
                                    [&charlabel, labelMeasurement, relativeX](int x, int y) {
                                        SetWindowPos(charlabel.window.get(), nullptr, x + relativeX, std::max(30, y), labelMeasurement.width().c, labelMeasurement.height().c, SWP_NOOWNERZORDER);
                                        InvalidateRect(charlabel.window.get(), nullptr, true);
                                        UpdateWindow(charlabel.window.get());
                                    }),
                                // on completed
                                    [](){},
                                // on error
                                    [this](const std::exception_ptr& e){
                                        exceptions->push(std::make_pair("error in label onmove stream: ", e));}));

                            relativeX += labelMeasurement.width().c;
                        }
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in text change stream: ", e));});

            auto msg = L"Time flies like an arrow";
            //auto msg = L"Hello";
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
        int maxHeight;
        std::shared_ptr<rx::Subject<std::wstring>> text;
        rxmsg::message::Subject messages;
        l::wr::unique_destroy_window edit;
        rxmsg::subclass_message::Subject editMessages;
        rxmsg::set_subclass_result editSubclass;
        std::shared_ptr<rx::ComposableDisposable> cd;
        std::list<label> labels;
        std::list<LabelCounts> counts;
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

