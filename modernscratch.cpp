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

namespace rxmsg {
// rx window message allows message handling via Rx
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

template<class Tag>
struct window_size : public std::enable_shared_from_this<window_size<Tag>> {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::coordinate coordinate;
    typedef typename traits::point point;
    typedef typename traits::extent extent;
    typedef std::shared_ptr<rx::Observer<coordinate>> ObserverCoordinate;
    typedef std::shared_ptr<rx::Observer<point>> ObserverPoint;
    typedef std::shared_ptr<rx::Observer<extent>> ObserverExtent;
    typedef std::shared_ptr<rx::Subject<coordinate>> SubjectCoordinate;
    typedef std::shared_ptr<rx::Subject<point>> SubjectPoint;
    typedef std::shared_ptr<rx::Subject<extent>> SubjectExtent;

    window_size(HWND window, message::Observable messages) : window(window), messages(messages), subscribed(0) {
    }

    rx::Disposable subscribeOrigin(const ObserverPoint& observer) {
        return subscribe(Origin, observer);}
    rx::Disposable subscribeExtent(const ObserverExtent& observer) {
        return subscribe(Extent, observer);}
    rx::Disposable subscribeLeft(const ObserverCoordinate& observer) {
        return subscribe(Left, observer);}
    rx::Disposable subscribeTop(const ObserverCoordinate& observer) {
        return subscribe(Top, observer);}
    rx::Disposable subscribeRight(const ObserverCoordinate& observer) {
        return subscribe(Right, observer);}
    rx::Disposable subscribeBottom(const ObserverCoordinate& observer) {
        return subscribe(Bottom, observer);}
 
private:
    template<class Subject, class Observer>
    rx::Disposable subscribe(const Subject& subject, const Observer& observer) {
        auto keepAlive = this->shared_from_this();
        if (++subscribed == 1) {
            sd.Set(rx::from(messages)
                .where([keepAlive, this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_WINDOWSPOSCHANGED;})
                .select([](const rxmsg::message& m){
                    set_handled(m);
                    return *(WINDOWPOS*)m.lParam;})
                .subscribe(
                    [keepAlive, this](const WINDOWPOS& pos){
                        auto x = coodinate(pos.x);
                        auto y = coodinate(pos.y);
                        auto cx = coodinate(pos.cx);
                        auto cy = coodinate(pos.cy);
                        Origin.OnNext(point(x,y));
                        Extent.OnNext(extent(cx, cy));
                        Left.OnNext(x);
                        Top.OnNext(y);
                        Right.OnNext(x+cx);
                        Bottom.OnNext(y+cy);},
                    [keepAlive, this](){
                        Origin.OnCompleted();
                        Extent.OnCompleted();
                        Left.OnCompleted();
                        Right.OnCompleted();
                        Top.OnCompleted();
                        Bottom.OnCompleted();}, 
                    [keepAlive, this](const std::exception_ptr& e){
                        Origin.OnError(e);
                        Extent.OnError(e);
                        Left.OnError(e);
                        Right.OnError(e);
                        Top.OnError(e);
                        Bottom.OnError(e);
                }));
        }
        rx::ComposableDisposable cd;
        cd.Add(rx::Disposable([keepAlive, this](){
            if(--subscribed == 0){
                sd.Dispose();}}));
        cd.Add(subject->Subscribe(observable));
        return cd;
    }

    message::Observable messages;
    HWND window;
    std::atomic<int> subscribed;
    rx::SharedDisposable sd;
    SubjectPoint Origin;
    SubjectExtent Extent;
    SubjectCoordinate Left;
    SubjectCoordinate Top;
    SubjectCoordinate Right;
    SubjectCoordinate Bottom;
};

}

namespace rxmsg{namespace traits{
    template<class Coordinate, class Point, class Extent>
    struct window_traits_builder {
        typedef Coordinate coordinate;
        struct point {
            Point p;
            point(coordinate x, coordinate y) {p.x = x; p.y = y;}
            explicit point(const Point& p) : p(p) {}
            operator Point&() {return p;}
            operator const Point&() const {return p;}
        };
        struct extent {
            Extent e;
            extent(coordinate cx, coordinate cy) {e.cx = cx; e.cy = cy;}
            explicit extent(const Extent& e) : e(e) {}
            operator Extent&() {return e;}
            operator const Extent&() const {return e;}
        };
    };

    struct Default {};
}}
rxmsg::traits::window_traits_builder<UINT, POINT, SIZE> rx_window_traits(rxmsg::traits::Default&&);

bool operator==(POINT lhs, POINT rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
bool operator!=(POINT lhs, POINT rhs) {return !(lhs == rhs);}

namespace RootWindow
{
    // tie the ADL methods together
    struct tag {};

    typedef
        l::wnd::Context<tag>
    Context;

    // note: no base class
    struct window
    {
        window(HWND handle, CREATESTRUCT& cs)
            : messages(rx::CreateSubject<rxmsg::message>())
            , size(handle, messages)
        {

            // post quit message
            rx::from(messages)
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_NCDESTROY;})
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    cd.Dispose();
                    PostQuitMessage(0);
                });

            // print client
            rx::from(messages)
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_PRINTCLIENT;})
                .select([](const rxmsg::message& m) {
                    return std::make_tuple(m.window, reinterpret_cast<HDC>(m.wParam), m.out);})
                .subscribe([this](const std::tuple<HWND, HDC, l::wnd::dispatch_result*>& m){
                    rxmsg::set_handled(std::get<2>(m));
                    PAINTSTRUCT ps = {};
                    ps.hdc = std::get<1>(m);
                    GetClientRect(std::get<0>(m), &ps.rcPaint);
                    rxmsg::set_lResult(
                        std::get<2>(m),
                        this->PaintContent(ps)
                    );
                });

            // paint
            rx::from(messages)
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_PAINT;})
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    PAINTSTRUCT ps = {};
                    BeginPaint(m.window, &ps);
                    l::wr::unique_gdi_end_paint ender(std::make_pair(m.window, &ps));
                    set_lResult(
                        m,
                        this->PaintContent(ps)
                    );
                });

            // disable erase background
            rx::from(messages)
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_ERASEBKGND;})
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    set_lResult(m,TRUE);
                });

            // note: distinct_until_changed is necessary; while 
            //  Winforms filters duplicate mouse moves, 
            //  user32 doesn't filter this for you: http://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx

            auto mouseMove = rx::from(messages)
                .where([this](const rxmsg::message& m) {
                    return !handled(m) && m.id == WM_MOUSEMOVE;})
                .select([](const rxmsg::message& m) {
                    POINT p = {GET_X_LPARAM(m.lParam), GET_Y_LPARAM(m.lParam)}; return p;})
                .distinct_until_changed()
                .publish();

            // set up labels and query
            auto msg = L"Time flies like an arrow";

            auto mainFormScheduler = std::make_shared<rx::win32::WindowScheduler>();

#if DELAY_ON_WORKER_THREAD
            auto worker = std::make_shared<rx::EventLoopScheduler>();
#endif

            for (int i = 0; msg[i]; ++i)
            {
                auto label = CreateLabelFromLetter(msg[i], cs.hInstance, handle);

                auto s = rx::from(mouseMove)
#if DELAY_ON_WORKER_THREAD
                    // delay on worker thread
                    .delay(std::chrono::milliseconds(i * 100 + 1), worker)
                    .observe_on(mainFormScheduler)
#else
                    // delay on ui thread
                    .delay(std::chrono::milliseconds(i * 100 + 1), mainFormScheduler)
#endif
                    .subscribe([=](const POINT& p) {
                        SetWindowPos(label, nullptr, p.x+20*i, p.y-20, 20, 30, SWP_NOOWNERZORDER);
                        InvalidateRect(label, nullptr, true);
                        UpdateWindow(label);
                    });

                cd.Add(std::move(s));
            }
        }

        inline HWND CreateLabelFromLetter(wchar_t c, HINSTANCE hinst, HWND parent)
        {
            unique_winerror winerror;
            std::pair<std::wstring, l::wr::unique_destroy_window> label;

            label.first.append(&c, &c+1);
            std::tie(winerror, label.second) = 
                l::wr::winerror_and_destroy_window(
                    CreateWindow(
                        L"Static", label.first.c_str(), 
                        WS_CHILD | WS_VISIBLE,
                        0, 0, 20, 30, 
                        parent, NULL, 
                        hinst, 
                        NULL));

            if (!winerror || !label.second)
            {
                winerror.throw_if("CreateWindow failed");
            }

            auto result = label.second.get();
            labels.push_back(std::move(label));
            return result;
        }

        LRESULT PaintContent(PAINTSTRUCT& ps)
        {
            l::wr::unique_gdi_brush gray(CreateSolidBrush(RGB(240,240,240)));
            FillRect(ps.hdc, &ps.rcPaint, gray.get());
            return 0;
        }

        rxmsg::message::Subject messages;
        rxmsg::window_size<rxmsg::traits::Default> size;
        rx::ComposableDisposable cd;
        std::list<std::pair<std::wstring, l::wr::unique_destroy_window>> labels;
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

    RootWindowClass::Register(L"Scratch");

    unique_winerror winerror;
    l::wr::unique_destroy_window window;

    std::tie(winerror, window) = 
        l::wr::winerror_and_destroy_window(
            CreateWindow(
                L"Scratch", L"Scratch", 
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                NULL, NULL, 
                hinst, 
                NULL));

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
    }
    return 0;
}

