#pragma once


namespace rxmsg {
// allows win32 message handling via Rx

namespace rx=rxcpp;
namespace l=LIBRARIES_NAMESPACE;

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

#if RXCPP_USE_VARIADIC_TEMPLATES
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
#else
struct no_type {};
template<class Tag, unsigned int Id, class T1 = no_type, class T2 = no_type, class T3 = no_type, class T4 = no_type>
struct message_trait_builder;

template<class Tag, unsigned int Id>
struct message_trait_builder<Tag, Id, no_type, no_type, no_type, no_type> {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND) -> detail::extract_lresult {
            result = args_type(m); return detail::extract_lresult();}));
        return result;
    }
};
template<class Tag, unsigned int Id, class T1>
struct message_trait_builder<Tag, Id, T1, no_type, no_type, no_type> {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<T1, Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND, T1 t) -> detail::extract_lresult {
            result = args_type(t, m); return detail::extract_lresult();}));
        return result;
    }
};
template<class Tag, unsigned int Id, class T1, class T2>
struct message_trait_builder<Tag, Id, T1, T2, no_type, no_type> {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<T1, T2, Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND, T1 t1, T2 t2) -> detail::extract_lresult {
            result = args_type(t1, t2, m); return detail::extract_lresult();}));
        return result;
    }
};
template<class Tag, unsigned int Id, class T1, class T2, class T3>
struct message_trait_builder<Tag, Id, T1, T2, T3, no_type> {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<T1, T2, T3, Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND, T1 t1, T2 t2, T3 t3) -> detail::extract_lresult {
            result = args_type(t1, t2, t3, m); return detail::extract_lresult();}));
        return result;
    }
};
template<class Tag, unsigned int Id, class T1, class T2, class T3, class T4>
struct message_trait_builder<Tag, Id, T1, T2, T3, T4> {
    typedef Tag tag;
    static const unsigned int id = Id;

    template<class Message>
    struct args {
        typedef std::tuple<T1, T2, T3, T4, Message> type;
    };

    template<class Message>
    static typename args<Message>::type crack(Message m) {
        typedef typename args<Message>::type args_type;
        args_type result;
        set_lResult(m, rxmsg_crack(tag(), m, [&](HWND, T1 t1, T2 t2, T3 t3, T4 t4) -> detail::extract_lresult {
            result = args_type(t1, t2, t3, t4, m); return detail::extract_lresult();}));
        return result;
    }
};
#endif

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

}