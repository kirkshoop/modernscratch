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

template<class Tag>
struct window_size : public std::enable_shared_from_this<window_size<Tag>> {
    typedef Tag tag;
    typedef decltype(rx_window_traits(tag())) traits;
    typedef typename traits::Message Message;
    typedef typename traits::Coordinate Coordinate;
    typedef typename traits::Point Point;
    typedef typename traits::Extent Extent;
    typedef std::shared_ptr<rx::Observer<Coordinate>> ObserverCoordinate;
    typedef std::shared_ptr<rx::Observer<Point>> ObserverPoint;
    typedef std::shared_ptr<rx::Observer<Extent>> ObserverExtent;
    typedef std::shared_ptr<rx::Observable<Coordinate>> ObservableCoordinate;
    typedef std::shared_ptr<rx::Observable<Point>> ObservablePoint;
    typedef std::shared_ptr<rx::Observable<Extent>> ObservableExtent;
    typedef std::shared_ptr<rx::Observable<std::tuple<Coordinate, Coordinate, Coordinate, Coordinate>>> ObservableCoordinateBox;
    typedef std::shared_ptr<rx::BehaviorSubject<Coordinate>> SubjectCoordinate;
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
            .subscribe(rxcpp::MakeTupleDispatch([keepAlive](Point p, Extent e){
                if (e.cx != 0 && e.cy != 0) {
                    SetWindowPos(window, nullptr, p.x, p.y, e.cx, e.y, SWP_NOOWNERZORDER);
                    InvalidateRect(window, nullptr, true);
                    UpdateWindow(window);
                }
            })));
        return sdBind;
    }
    rx::Disposable bind(const ObservableCoordinate& l, const ObservableCoordinate& t, const ObservableCoordinate& r, const ObservableCoordinate& b) {
        auto keepAlive = this->shared_from_this();
        sdBind.Dispose();
        sdBind.Set(rx::from(l)
            .combine_latest(t, r, b)
            .subscribe(rxcpp::MakeTupleDispatch([keepAlive, this](Coordinate l, Coordinate t, Coordinate r, Coordinate b){
                if (b-t != 0 && r-l != 0) {
                    SetWindowPos(window, nullptr, l, t, r - l, b - t, SWP_NOOWNERZORDER);
                    InvalidateRect(window, nullptr, true);
                    UpdateWindow(window);
                }
            })));
        return sdBind;
    }
    rx::Disposable bind(const ObservableCoordinateBox& b) {
        auto keepAlive = this->shared_from_this();
        sdBind.Dispose();
        sdBind.Set(rx::from(b)
            .subscribe(rxcpp::MakeTupleDispatch([keepAlive, this](Coordinate l, Coordinate t, Coordinate r, Coordinate b){
                if (b-t != 0 && r-l != 0) {
                    SetWindowPos(window, nullptr, l, t, r - l, b - t, SWP_NOOWNERZORDER);
                    InvalidateRect(window, nullptr, true);
                    UpdateWindow(window);
                }
            })));
        return sdBind;
    }

private:
    void initSubjects() {
        subjectOrigin = rx::CreateBehaviorSubject<Point>(Point());
        subjectExtent = rx::CreateBehaviorSubject<Extent>(Extent());
        subjectLeft = rx::CreateBehaviorSubject<Coordinate>(Coordinate());
        subjectTop = rx::CreateBehaviorSubject<Coordinate>(Coordinate());
        subjectRight = rx::CreateBehaviorSubject<Coordinate>(Coordinate());
        subjectBottom = rx::CreateBehaviorSubject<Coordinate>(Coordinate());
    }
    void updateSubjects() {
        RECT client = {};
        if (!parent) {
            GetClientRect(window, &client);
        }
        else {
            GetWindowRect(window, &client);
            MapWindowPoints(NULL, parent, (LPPOINT)&client, 2);
        }
        Coordinate x = Coordinate(client.left);
        Coordinate y = Coordinate(client.top);
        Coordinate cx = Coordinate(client.right) - x;
        Coordinate cy = Coordinate(client.bottom) - y;
        subjectOrigin->OnNext(Point(x,y));
        subjectExtent->OnNext(Extent(cx, cy));
        subjectLeft->OnNext(x);
        subjectTop->OnNext(y);
        subjectRight->OnNext(Coordinate(client.right));
        subjectBottom->OnNext(Coordinate(client.bottom));
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
                cd.Add(subject->Subscribe(observer));
                return cd;
            });
    }

    typename Message::Observable messages;
    HWND parent;
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
    template<class CoordinateArg, class PointArg, class ExtentArg, class MessageArg>
    struct window_traits_builder {
        typedef CoordinateArg Coordinate;
        typedef MessageArg Message;
        struct Point {
            PointArg p;
            Point() {p.x = 0; p.y = 0;}
            Point(Coordinate x, Coordinate y) {p.x = x; p.y = y;}
            explicit Point(const PointArg& p) : p(p) {}
            operator PointArg&() {return p;}
            operator const PointArg&() const {return p;}
        };
        struct Extent {
            ExtentArg e;
            Extent() {e.cx = 0; e.cy = 0;}
            Extent(Coordinate cx, Coordinate cy) {e.cx = cx; e.cy = cy;}
            explicit Extent(const ExtentArg& e) : e(e) {}
            operator ExtentArg&() {return e;}
            operator const ExtentArg&() const {return e;}
        };
    };

    struct OwnDefault {};
    window_traits_builder<int, POINT, SIZE, message> rx_window_traits(OwnDefault&&);

    struct SubclassDefault {};
    window_traits_builder<int, POINT, SIZE, subclass_message> rx_window_traits(SubclassDefault&&);
}}
using rxmsg::traits::rx_window_traits;

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
        typedef window_size::Coordinate Coordinate;
        typedef window_size::SubjectCoordinate SubjectCoordinate;

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
        window(HWND handle, CREATESTRUCT& cs)
            : root(handle)
            , exceptions(reinterpret_cast<Exceptions*>(cs.lpCreateParams))
            , text(rx::CreateSubject<std::wstring>())
            , messages(rx::CreateSubject<rxmsg::message>())
            , size(std::make_shared<rxmsg::window_size<rxmsg::traits::OwnDefault>>(HWND_DESKTOP, handle, messages))
            , edit(CreateChildWindow(L"Edit", L"", POINT(), cs.hInstance, handle))
            , editMessages(rx::CreateSubject<rxmsg::subclass_message>())
            , editSize(std::make_shared<rxmsg::window_size<rxmsg::traits::SubclassDefault>>(handle, edit.get(), editMessages))
            , editSubclass(rxmsg::set_window_subclass(edit.get(), editMessages))
//            , labelX(L"0", POINT(), handle, cs.hInstance)
//            , labelY(L"0", POINT(), handle, cs.hInstance)
            , mouseX(rx::CreateBehaviorSubject<typename label::Coordinate>(0))
            , mouseY(rx::CreateBehaviorSubject<typename label::Coordinate>(0))
        {
            typedef rxmsg::window_size<rxmsg::traits::SubclassDefault> edit_size;
#if 0
            labelY.size->bind(
                rxcpp::from(labelX.size->right())
                    .combine_latest(size->top(), size->right())
                    .select(rxcpp::MakeTupleDispatch([](label::Coordinate l, label::Coordinate t, label::Coordinate r){
                        return std::make_tuple(l, t, r, t+30);}))
                    .publish());

            labelX.size->bind(
                rxcpp::from(editSize->right())
                    .combine_latest(size->top(), size->right())
                    .select(rxcpp::MakeTupleDispatch([](label::Coordinate l, label::Coordinate t, label::Coordinate r){
                        return std::make_tuple(l, t, l + ((r - l) / 2), t+30);}))
                    .publish());

            editSize->bind(
                rxcpp::from(size->left())
                    .combine_latest(size->top(), size->right())
                    .select(rxcpp::MakeTupleDispatch([](edit_size::Coordinate l, edit_size::Coordinate t, edit_size::Coordinate r){
                        return std::make_tuple(l, t, l + ((r - l) / 2), t+30);}))
                    .publish());
#else
            editSize->bind(
                rxcpp::from(size->left())
                    .combine_latest(size->top(), size->right())
                    .select(rxcpp::MakeTupleDispatch([](edit_size::Coordinate l, edit_size::Coordinate t, edit_size::Coordinate r){
                        return std::make_tuple(l, t, r, t+30);}))
                    .publish());

#endif

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

            // set up labels and query

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
                    mouseX->OnNext(x); mouseY->OnNext(y);}));

            rx::from(text)
                .subscribe(
                // on next
                    [=](const std::wstring& msg){
                        cd.Dispose();
                        labels.clear();

                        auto left = rxcpp::observable(mouseX);
                        auto bottom = rxcpp::observable(mouseY);

                        for (int i = 0; msg[i]; ++i)
                        {
                            POINT loc = {20 * i, 30};
                            auto& charlabel = CreateLabelFromLetter(msg[i], loc, cs.hInstance, handle);

                            cd.Add(charlabel.size->bind(
                                rxcpp::from(left)
                                    .select([i](label::Coordinate c){
                                            return c;})
                                    .zip( 
                                        rxcpp::from(bottom).select([i](label::Coordinate c){
                                            return std::max(30, c-30);}).publish(),
                                        rxcpp::from(left).select([i](label::Coordinate c){
                                            return c+20;}).publish(),
                                        rxcpp::from(bottom).select([i](label::Coordinate c){
                                            return std::max(30 + 30, c);}).publish())
                                    // delay on worker thread
                                    .delay(std::chrono::milliseconds(100), worker)
                                    .observe_on(mainFormScheduler)
                                    .publish()));

                            left = charlabel.size->right();
                            bottom = charlabel.size->bottom();
                        }
                    },
                // on completed
                    [](){},
                //on error
                    [this](const std::exception_ptr& e){
                        exceptions->push(std::make_pair("error in label onmove stream: ", e));});

            auto msg = L"Time flies like an arrow";
            //auto msg = L"Hi";
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
//        label labelX;
//        label labelY;
        std::list<label> labels;
        label::SubjectCoordinate mouseX;
        label::SubjectCoordinate mouseY;
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

