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
#pragma comment(lib, "uxtheme.lib")

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Ole32.lib")

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

#if 0
#include <http_client.h>
namespace wh = web::http;
namespace whc = web::http::client;
#endif

#include "cpprx/rx.hpp"

#define UNIQUE_WINERROR_DEFINE_REPORTS
#define UNIQUE_HRESULT_DEFINE_REPORTS
#define LIBRARIES_NAMESPACE mylib
#include "libraries.h"

#if 0
#include "rxanimate/rxanimate.hpp"
#endif

#include "rxwin32/rxwin32.hpp"
namespace rxmsr=rxmeasurement;

namespace rx=rxcpp;
namespace l=LIBRARIES_NAMESPACE;


typedef std::queue<std::pair<std::string, std::exception_ptr>> Exceptions;

#if 0
std::shared_ptr<rx::Observable<wh::http_response>> HttpRequest(const std::shared_ptr<rx::Observable<wh::http_request>>& source) {
    return rx::CreateObservable<wh::http_response>(
        [=](std::shared_ptr<rx::Observer<wh::http_response>> observer) 
            -> rx::Disposable {
            struct State {
                State() : requestsPending(0) {
                }
                std::mutex lock;
                size_t requestsPending;
                bool done;
                bool error;
            };
            auto state = std::make_shared<State>();
            return rx::from(source)
                .subscribe(
                //on next
                    [=](const wh::http_request& rq){
                        try{
                            whc::http_client client(rq.request_uri());
                            {std::unique_lock<std::mutex> guard(state->lock); ++state->requestsPending;}
                            client.request(rq)
                                .then([=](wh::http_response rs){
                                    observer->OnNext(std::move(rs));
                                    bool done = false;
                                    {std::unique_lock<std::mutex> guard(state->lock); done = (0 == --state->requestsPending) && !state->error && state->done;}
                                    if (done) {
                                        observer->OnCompleted();
                                    }
                                });
                        } catch (...) {
                            observer->OnError(std::current_exception());
                        }
                    },
                // on completed
                    [=](){
                        bool done = false;
                        {std::unique_lock<std::mutex> guard(state->lock); state->done = true; done = !state->error && 0 == state->requestsPending;}
                        if (done) {
                            observer->OnCompleted();
                        }
                    },
                // on error
                    [=](const std::exception_ptr& error){
                        {std::unique_lock<std::mutex> guard(state->lock); state->error = true;}
                        observer->OnError(error);
                    });
        });
}

struct http_request {};
auto rxcpp_chain(http_request&&, 
    const std::shared_ptr<rx::Observable<wh::http_request>>& source) 
    -> decltype(HttpRequest(source)) {
    return      HttpRequest(source);
}
#endif

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
        typedef rxmsr::window_measure<rxmsr::traits::Default> label_measure;
        typedef rxtheme::label<rxmsr::traits::Default, rxmsg::subclass_message> text_measure;
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

    struct tile : public std::enable_shared_from_this<tile>
    {
    public:
        tile(std::wstring text, POINT p, HWND parent, HINSTANCE hInstance) : 
            title(text, p, parent, hInstance) {
        }
        typedef std::shared_ptr<tile> shared;
        label title;
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
        typedef rx::Scheduler::clock clock;
        typedef rxmsr::window_measure<rxmsr::traits::Default> top_measure;
#if 0
        typedef rxanim::time_animation<clock> time_animation;
        typedef time_animation::adjust_type adjust_type;
        typedef time_animation::state_type state_type;
        typedef time_animation::time_range time_range;
        typedef time_animation::time_point time_point;
        typedef time_animation::duration_type time_duration;
#endif
        window(HWND handle, CREATESTRUCT& cs)
            : root(handle)
            , instance(cs.hInstance)
            , exceptions(reinterpret_cast<Exceptions*>(cs.lpCreateParams))
            , text(rx::CreateSubject<std::wstring>())
            , tilesChanged(rx::CreateBehaviorSubject<size_t>(0))
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
                .select([](size_t ){return clock::now();});

            auto rootMeasurement = rx::from(messages)
                .chain<top_measure::select_client_measurement>()
                .distinct_until_changed();

            auto editBounds = rx::from(rootMeasurement)
                .select([](top_measure::Measurement m){
                    return std::make_tuple(m.left.c, m.top.c, m.width().c);});
#if 0
            auto editta = time_animation(
                state_type(rxanim::runOnce<time_range, time_point>), 
                adjust_type(rxanim::adjustNone<time_range, time_point>),
                rxanim::ease_type(rxanim::easeBounceDamp));

            auto ta = time_animation(
                state_type(rxanim::runN<3, time_range, time_point>), 
                adjust_type(rxanim::adjustPingPong<time_range, time_point>),
                rxanim::ease_type(rxanim::easeSquareRoot));
#endif
            rx::from(editBounds)
#if 0
                .chain<rxanim::animate>(updateInterval, editta,
                    [=](const std::tuple<int, int, int>& ){
                        RECT rc = {};
                        GetClientRect(edit.get(), &rc);
                        return std::make_tuple(rc.left, rc.top, rc.right - rc.left);},
                    [=](time_point , time_point animateFinish, const std::tuple<int, int, int>&, const std::tuple<int, int, int>&){
                        return rxanim::time_range<clock>(
                            animateFinish, 
                            animateFinish + std::chrono::seconds(2));})
#endif
                .observe_on(mainFormScheduler)
                .subscribe(
                    rx::MakeTupleDispatch([this](int left, int top, int width){
                        SetWindowPos(edit.get(), nullptr, left, top, width, 30, SWP_NOOWNERZORDER);
                        InvalidateRect(edit.get(), nullptr, true);
                        UpdateWindow(edit.get());
                    }));

#if 0
            requests.push_back(wh::http_request(wh::methods::GET));
            requests.back().set_request_uri(L"http://news.google.com/news?pz=1&cf=all&ned=us&hl=en&output=rss");
            requests.push_back(wh::http_request(wh::methods::GET));
            requests.back().set_request_uri(L"http://news.google.com/news?pz=1&cf=all&ned=us&hl=en&output=atom");

            // layout tiles
            rx::from(editMessages)
                .chain<top_measure::select_parent_measurement>()
                .select([](top_measure::Measurement m){
                    return std::make_tuple(m.left.c, m.bottom.c);})
                .combine_latest([](std::tuple<int, int> leftTop, top_measure::Measurement root, size_t ){
                    return std::tuple_cat(leftTop, std::make_tuple(root));}, rootMeasurement, rx::observable(tilesChanged))
                .observe_on(mainFormScheduler)
                .subscribe(rx::MakeTupleDispatch([this](int left, int top, top_measure::Measurement root){
                    if(cdTiles) {
                        cdTiles->Dispose();}
                    cdTiles = std::make_shared<rx::ComposableDisposable>();
                    int nextTop = top;
                    int nextLeft = left;
                    int maxWidth = 0;
                    std::unique_lock<std::mutex> guard(tilesLock);
                    for(auto& tile : tiles) {
                        auto textMeasurement = tile->title.textmeasure.measureText();
                        SetWindowPos(tile->title.window.get(), nullptr, left, top, textMeasurement.width().c, textMeasurement.height().c, SWP_NOOWNERZORDER);
                        InvalidateRect(tile->title.window.get(), nullptr, true);
                        UpdateWindow(tile->title.window.get());
                        maxWidth = std::max(maxWidth, textMeasurement.width().c);
                        if (nextTop + textMeasurement.height().c > root.height().c){
                            nextTop = top;
                            nextLeft += maxWidth;
                            maxWidth = 0;
                        } else {
                            nextTop += textMeasurement.height().c;
                        }

                        if (nextLeft > root.width().c) {
                            break;
                        }
                    }
                }));

            auto requestSource = std::make_shared<rx::EventLoopScheduler>();
            rx::from(rx::Interval(std::chrono::seconds(5), requestSource))
                .select_many([=, this](size_t ){
                    return rx::from(Iterate(requests, worker))
                        .chain<http_request>();
                    },
                    [=, this](size_t, wh::http_response rs) {
                        std::unique_lock<std::mutex> guard(tilesLock);
                        tiles.push_back(std::make_shared<tile>(rs.reason_phrase(), POINT(), root, instance));
                        return 1;
                    })
                .observe_on(mainFormScheduler)
                .subscribe(
                // on next
                    [=](size_t chng){
                        tilesChanged->OnNext(chng);},
                // on completed
                    [](){
                        },
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in http request: ", e));});
#endif

            auto commands = rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::command>())
                .select([](const rxmsg::message& msg){
                    return rxmsg::handle_message<rxmsg::wm::command>(msg);});

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
                .where(rxmsg::messageId<rxmsg::wm::lbuttondown>());

            auto mouseUp = rx::from(messages)
                .where(rxmsg::messageId<rxmsg::wm::lbuttonup>());

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
                        .distinct_until_changed();})
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
                        past_labels.push_back(labels);
                        labels = std::make_shared<std::list<label>>();
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
                                p.p.y -= maxHeight; return std::tuple<int, int>(p.p.x, p.p.y);});

                        for (int i = 0; msg[i]; ++i)
                        {
                            POINT loc = {20 * i, 30};
                            auto& charlabel = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);
                            auto labelMeasurement = charlabel.textmeasure.measureText();
                            counts.emplace_back();
                            auto& c = counts.back();
                            c.text.append(msg.begin() + i, msg.begin() + i + 1);

                            maxHeight = std::max(maxHeight, labelMeasurement.height().c);
                            auto keepAlive = labels;

                            cd->Add(rx::from(point)
                                .chain<record>(&c.source)
                                .distinct_until_changed()
                                .chain<record>(&c.distinct)
#if 0
                                .chain<rxanim::animate>(updateInterval, ta,
                                    [&charlabel](const std::tuple<int, int>& animateFinal){
                                        return animateFinal;},
                                    [=](time_point now, time_point, const std::tuple<int, int>&, const std::tuple<int, int>&){
                                        return rxanim::time_range<clock>(
                                            now + std::chrono::milliseconds(100 * (i-1)), 
                                            now + std::chrono::milliseconds(100 * i));})
                                .chain<record>(&c.animated)
#else
                                .delay(std::chrono::milliseconds(100 * i), worker)
#endif
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
                                    [keepAlive](){ // using the capture to keep labels alive until the stream completes
                                        },
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

            labels->emplace_back(std::move(text), p, parent, hinst);
            return labels->back();
        }

        LRESULT PaintContent(PAINTSTRUCT& ps)
        {
            ps.rcPaint.top = std::max(0L, ps.rcPaint.top);
            l::wr::unique_gdi_brush gray(CreateSolidBrush(RGB(240,240,240)));
            FillRect(ps.hdc, &ps.rcPaint, gray.get());
            return 0;
        }

        HWND root;
        HINSTANCE instance;
        Exceptions* exceptions;
        int maxHeight;
        std::shared_ptr<rx::Subject<std::wstring>> text;
        rxmsg::message::Subject messages;
        l::wr::unique_destroy_window edit;
        rxmsg::subclass_message::Subject editMessages;
        rxmsg::set_subclass_result editSubclass;
        std::shared_ptr<rx::ComposableDisposable> cd;
        std::shared_ptr<std::list<label>> labels;
        std::vector<std::weak_ptr<std::list<label>>> past_labels;
        std::list<LabelCounts> counts;
        label::SubjectPoint mousePoint;
        std::mutex tilesLock;
        std::vector<tile::shared> tiles;
        std::shared_ptr<rx::BehaviorSubject<size_t>> tilesChanged;
        std::shared_ptr<rx::ComposableDisposable> cdTiles;
#if 0
        std::vector<wh::http_request> requests;
#endif
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

