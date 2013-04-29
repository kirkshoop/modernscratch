#pragma once

namespace rxmsg {namespace wm {
namespace rx=rxcpp;

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
}}
using rxmsg::wm::detail::rxmsg_traits;
using rxmsg::wm::detail::rxmsg_crack;
