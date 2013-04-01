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
    typedef typename traits::Coordinate Coordinate;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef std::shared_ptr<rx::Observer<Coordinate>> ObserverCoordinate;
    typedef std::shared_ptr<rx::Observer<Point>> ObserverPoint;
    typedef std::shared_ptr<rx::Observer<Extent>> ObserverExtent;
    typedef std::shared_ptr<rx::Observable<Coordinate>> ObservableCoordinate;
    typedef std::shared_ptr<rx::Observable<Point>> ObservablePoint;
    typedef std::shared_ptr<rx::Observable<Extent>> ObservableExtent;
    typedef std::shared_ptr<rx::Subject<Coordinate>> SubjectCoordinate;
    typedef std::shared_ptr<rx::Subject<Point>> SubjectPoint;
    typedef std::shared_ptr<rx::Subject<Extent>> SubjectExtent;

    window_size(HWND window, message::Observable messages) : window(window), messages(messages), subscribed(0) {
        initSubjects();
    }
    window_size(HWND window, message::Subject messages) : window(window), messages(messages), subscribed(0) {
        initSubjects();
    }

    ObservablePoint origin() {
        return create(subjectOrigin);}
    ObservableExtent extent() {
        return create(subjectExtent);}
    ObservableCoordinate left() {
        return create(subjectLeft);}
    ObservableCoordinate top() {
        return create(subjectTop);}
    ObservableCoordinate right() {
        return create(subjectRight);}
    ObservableCoordinate bottom() {
        return create(subjectBottom);}
 
    rx::Disposable bind(const ObservablePoint& p, const ObservableExtent& e) {
        auto keepAlive = this->shared_from_this();
        sdBind.Dispose();
        sdBind.Set(rx::from(p)
            .combine_latest(e)
            .subscribe([keepAlive, this](const std::tuple<Point, Extent>& box){
                SetWindowPos(window, nullptr, std::get<0>(box).p.x, std::get<0>(box).p.y, std::get<1>(box).e.cx, std::get<1>(box).e.cy, SWP_NOOWNERZORDER);
                //MoveWindow(window, std::get<0>(box).p.x, std::get<0>(box).p.y, std::get<1>(box).e.cx, std::get<1>(box).e.cy, TRUE);
                keepAlive->updateSubjects(std::get<0>(box).p.x, std::get<0>(box).p.y, std::get<0>(box).p.x + std::get<1>(box).e.cx, std::get<0>(box).p.y + std::get<1>(box).e.cy);
            }));
        return sdBind;
    }
    rx::Disposable bind(const ObservableCoordinate& l, const ObservableCoordinate& t, const ObservableCoordinate& r, const ObservableCoordinate& b) {
        auto keepAlive = this->shared_from_this();
        sdBind.Dispose();
        sdBind.Set(rx::from(l)
            .combine_latest(t, r, b)
            .subscribe([keepAlive, this](const std::tuple<Coordinate, Coordinate, Coordinate, Coordinate>& box){
                SetWindowPos(window, nullptr, std::get<0>(box), std::get<1>(box), std::get<2>(box) - std::get<0>(box), std::get<3>(box) - std::get<1>(box), SWP_NOOWNERZORDER);
                //MoveWindow(window, std::get<0>(box), std::get<1>(box), std::get<2>(box) - std::get<0>(box), std::get<3>(box) - std::get<1>(box), TRUE);
                keepAlive->updateSubjects(std::get<0>(box), std::get<1>(box), std::get<2>(box), std::get<3>(box));
            }));
        return sdBind;
    }

private:
    void initSubjects() {
        subjectOrigin = rx::CreateSubject<Point>();
        subjectExtent = rx::CreateSubject<Extent>();
        subjectLeft = rx::CreateSubject<Coordinate>();
        subjectTop = rx::CreateSubject<Coordinate>();
        subjectRight = rx::CreateSubject<Coordinate>();
        subjectBottom = rx::CreateSubject<Coordinate>();
    }
    void updateSubjects(Coordinate top, Coordinate left, Coordinate right, Coordinate bottom) {
        auto x = left;
        auto y = top;
        auto cx = right - left;
        auto cy = bottom - top;
        subjectOrigin->OnNext(Point(x,y));
        subjectExtent->OnNext(Extent(cx, cy));
        subjectLeft->OnNext(x);
        subjectTop->OnNext(y);
        subjectRight->OnNext(right);
        subjectBottom->OnNext(bottom);
    }
    template<class Subject>
    typename rx::subject_observable<Subject>::type create(const Subject& subject) {
        auto keepAlive = this->shared_from_this();
        return rx::CreateObservable<typename rx::subject_item<Subject>::type>(
            [keepAlive, this, subject](const typename rx::subject_observer<Subject>::type& observer) {
            if (++subscribed == 1) {
                sdMessages.Set(rx::from(messages)
                    .where([](const rxmsg::message& m){
                        return !handled(m) && m.id == WM_WINDOWPOSCHANGED;})
                    .select([](const rxmsg::message& m){
                        set_handled(m);
                        return *(WINDOWPOS*)m.lParam;})
                    .subscribe(
                    // on next
                        [keepAlive](const WINDOWPOS& pos){
                            RECT client = {};
                            GetClientRect(pos.hwnd, &client);
                            keepAlive->updateSubjects(
                                Coordinate(client.top), 
                                Coordinate(client.left), 
                                Coordinate(client.right), 
                                Coordinate(client.bottom));},
                    // on completed
                        [keepAlive](){
                            keepAlive->subjectOrigin->OnCompleted();
                            keepAlive->subjectExtent->OnCompleted();
                            keepAlive->subjectLeft->OnCompleted();
                            keepAlive->subjectRight->OnCompleted();
                            keepAlive->subjectTop->OnCompleted();
                            keepAlive->subjectBottom->OnCompleted();}, 
                    // on error
                        [keepAlive](const std::exception_ptr& e){
                            keepAlive->subjectOrigin->OnError(e);
                            keepAlive->subjectExtent->OnError(e);
                            keepAlive->subjectLeft->OnError(e);
                            keepAlive->subjectRight->OnError(e);
                            keepAlive->subjectTop->OnError(e);
                            keepAlive->subjectBottom->OnError(e);
                    }));
            }
            rx::ComposableDisposable cd;
            cd.Add(rx::Disposable([keepAlive](){
                if(--keepAlive->subscribed == 0){
                    keepAlive->sdMessages.Dispose();}}));
            auto filtered = rx::from(subject)
                .distinct_until_changed()
                .publish();
            cd.Add(filtered->Subscribe(observer));
            return cd;
        });
    }

    message::Observable messages;
    HWND window;
    std::atomic<int> subscribed;
    rx::SharedDisposable sdMessages;
    rx::SharedDisposable sdBind;
    SubjectPoint subjectOrigin;
    SubjectExtent subjectExtent;
    SubjectCoordinate subjectLeft;
    SubjectCoordinate subjectTop;
    SubjectCoordinate subjectRight;
    SubjectCoordinate subjectBottom;
};

template<class Tag>
class stack {
public:
private:
    std::vector<std::shared_ptr<window_size<Tag>>> stacked;
};

}

namespace rxmsg{namespace traits{
    template<class CoordinateArg, class PointArg, class ExtentArg>
    struct window_traits_builder {
        typedef CoordinateArg Coordinate;
        struct Point {
            PointArg p;
            Point(Coordinate x, Coordinate y) {p.x = x; p.y = y;}
            explicit Point(const PointArg& p) : p(p) {}
            operator PointArg&() {return p;}
            operator const PointArg&() const {return p;}
        };
        struct Extent {
            ExtentArg e;
            Extent(Coordinate cx, Coordinate cy) {e.cx = cx; e.cy = cy;}
            explicit Extent(const ExtentArg& e) : e(e) {}
            operator ExtentArg&() {return e;}
            operator const ExtentArg&() const {return e;}
        };
    };

    struct Default {};
}}
rxmsg::traits::window_traits_builder<int, POINT, SIZE> rx_window_traits(rxmsg::traits::Default&&);

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

    // note: no base class
    struct window
    {
        window(HWND handle, CREATESTRUCT& cs)
            : root(handle)
            , exceptions(reinterpret_cast<Exceptions*>(cs.lpCreateParams))
            , text(rx::CreateSubject<std::wstring>())
            , messages(rx::CreateSubject<rxmsg::message>())
            , size(std::make_shared<rxmsg::window_size<rxmsg::traits::Default>>(handle, messages))
            , edit(CreateChildWindow(L"Edit", L"", POINT(), cs.hInstance, handle))
            , editMessages(rx::CreateSubject<rxmsg::message>())
            , editSize(std::make_shared<rxmsg::window_size<rxmsg::traits::Default>>(edit.get(), editMessages))
        {
            mouseLoc.x = 0; 
            mouseLoc.y = 30;

            editSize->bind(
                size->left(),
                size->top(),
                size->right(),
                rx::from(size->top())
                    .select([&, handle](int c) -> int {return c + 30;})
                    .publish());

            // edit text changed
            rx::from(messages)
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_COMMAND && HIWORD(m.wParam) == EN_CHANGE && ((HWND)m.lParam) == edit.get();})
                .subscribe(
                // on next
                [this](const rxmsg::message& m){
                    set_handled(m);
                    auto length = Edit_GetTextLength(edit.get());
                    std::wstring line;
                    line.reserve(length + 1);
                    line.resize(length);
                    Edit_GetText(edit.get(), &line[0], length + 1);
                    text->OnNext(line);
                },
                // on completed
                [](){},
                //on error
                [this](const std::exception_ptr& e){
                    exceptions->push(std::make_pair("error in edit changed: ", e));});

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
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_PAINT;})
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
                .where([this](const rxmsg::message& m){
                    return !handled(m) && m.id == WM_ERASEBKGND;})
                .subscribe([this](const rxmsg::message& m){
                    set_handled(m);
                    set_lResult(m,TRUE);
                });

            // set up labels and query

#if DELAY_ON_WORKER_THREAD
            auto worker = std::make_shared<rx::EventLoopScheduler>();
#endif
            auto mainFormScheduler = std::make_shared<rx::win32::WindowScheduler>();

            auto mouseDown = rx::from(messages)
                .where([this](const rxmsg::message& m) {
                    return !handled(m) && m.id == WM_LBUTTONDOWN;})
                .publish();

            auto mouseUp = rx::from(messages)
                .where([this](const rxmsg::message& m) {
                    return !handled(m) && m.id == WM_LBUTTONUP;})
                .publish();

            // note: distinct_until_changed is necessary; while 
            //  Winforms filters duplicate mouse moves, 
            //  user32 doesn't filter this for you: http://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx

            //this only produces mouse move events 
            //between an LButtonDown and the next LButtonUp
            auto mouseDrag = rx::from(mouseDown)
                .select_many([=, this](const rxmsg::message&) {
                    return rx::from(messages)
                        .take_until(mouseUp)
                        .where([this](const rxmsg::message& m) {
                            return !handled(m) && m.id == WM_MOUSEMOVE;})
                        .select([this](const rxmsg::message& m) {
                            POINT p = {GET_X_LPARAM(m.lParam), GET_Y_LPARAM(m.lParam)}; this->mouseLoc = p; return p;})
                        .distinct_until_changed()
                        .publish();})
                .publish();

            rx::from(text)
                .subscribe(
                // on next
                [=](const std::wstring& msg){
                    cd.Dispose();
                    labels.clear();

                    for (int i = 0; msg[i]; ++i)
                    {
                        POINT loc = {mouseLoc.x+20*i, std::max(30L, mouseLoc.y-20)};
                        auto label = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);

                        auto s = rx::from(mouseDrag)
                            // work around dispose bug
                            .where([=](const POINT& ) {
                                return IsWindow(label);})
#if DELAY_ON_WORKER_THREAD
                            // delay on worker thread
                            .delay(std::chrono::milliseconds(i * 100 + 1), worker)
                            .observe_on(mainFormScheduler)
#else
                            // delay on ui thread
                            .delay(std::chrono::milliseconds(i * 100 + 1), mainFormScheduler)
#endif
                            .subscribe(
                            // on next
                            [=](const POINT& p) {
                                SetWindowPos(label, nullptr, p.x+20*i, std::max(30L, p.y-20), 20, 30, SWP_NOOWNERZORDER);
                                InvalidateRect(label, nullptr, true);
                                UpdateWindow(label);
                            },
                            // on completed
                            [](){},
                            //on error
                            [this](const std::exception_ptr& e){
                                exceptions->push(std::make_pair("error in label onmove stream: ", e));});

                        cd.Add(std::move(s));
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

        inline HWND CreateLabelFromLetter(wchar_t c, POINT p, HINSTANCE hinst, HWND parent)
        {
            std::pair<std::wstring, l::wr::unique_destroy_window> label;

            label.first.append(&c, &c+1);
            label.second = 
                CreateChildWindow(
                    L"Static", label.first.c_str(), p,
                    hinst, parent);

            auto result = label.second.get();
            labels.push_back(std::move(label));
            return result;
        }

        LRESULT PaintContent(PAINTSTRUCT& ps)
        {
            ps.rcPaint.top = std::max(30L, ps.rcPaint.top);
            l::wr::unique_gdi_brush gray(CreateSolidBrush(RGB(240,240,240)));
            FillRect(ps.hdc, &ps.rcPaint, gray.get());
            return 0;
        }

        HWND root;
        Exceptions* exceptions;
        std::shared_ptr<rx::Subject<std::wstring>> text;
        rxmsg::message::Subject messages;
        std::shared_ptr<rxmsg::window_size<rxmsg::traits::Default>> size;
        l::wr::unique_destroy_window edit;
        rxmsg::message::Subject editMessages;
        std::shared_ptr<rxmsg::window_size<rxmsg::traits::Default>> editSize;
        rx::ComposableDisposable cd;
        std::list<std::pair<std::wstring, l::wr::unique_destroy_window>> labels;
        POINT mouseLoc;
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

