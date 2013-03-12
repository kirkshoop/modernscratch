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

#if 1
#include <new>
#include <utility>
#include <memory>
#include <type_traits>
#include <tuple>
#include <string>
#include <vector>

#define UNIQUE_WINERROR_DEFINE_REPORTS
#define UNIQUE_HRESULT_DEFINE_REPORTS
#define LIBRARIES_NAMESPACE mylib
#include "..\libraries\libraries.h"
namespace l=LIBRARIES_NAMESPACE;

namespace RootWindow
{
	struct tag {};

	typedef
		l::wnd::Context<tag>
	Context;

	struct window
	{
		explicit window(CREATESTRUCT&)
		{
		}

		l::wr::unique_close_window child;

		LRESULT OnSize(const Context& , UINT , int cx, int cy)
		{
			if (child) 
			{
				SetWindowPos(
					child.get(), NULL, 
					0, 0, cx, cy,
					SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
		}

		LRESULT PaintContent(PAINTSTRUCT& )
		{
			return 0;
		}

		LRESULT OnPaint(const Context& context)
		{
			PAINTSTRUCT ps = {};
			BeginPaint(context.window, &ps);
			l::wr::unique_gdi_end_paint ender(std::make_pair(context.window, &ps));
			return PaintContent(ps);
		}

		LRESULT OnPrintClient(const Context& context, HDC hdc, DWORD )
		{
			PAINTSTRUCT ps = {};
			ps.hdc = hdc;
			GetClientRect(context.window, &ps.rcPaint);
			return PaintContent(ps);
		}

		LRESULT OnNCDestroy(const Context& )
		{
			PostQuitMessage(0);
			return 0;
		}
	};

	l::wnd::traits_builder<window> window_class_traits(tag&&);

	void window_class_register(PCWSTR windowClass, WNDCLASSEX* wcex, tag&&)
	{
		wcex->style         = 0;
		wcex->hIcon         = NULL;
		wcex->hCursor       = LoadCursor(NULL, IDC_ARROW);
		wcex->lpszMenuName  = NULL;
		wcex->lpszClassName = windowClass;
	}
}

template<typename Function, typename MessageTag>
void window_message_error_contract(Function&& function, const RootWindow::Context& , MessageTag&&, RootWindow::tag&&)
{
	try
	{
		std::forward<Function>(function)();
	}
	catch(std::bad_alloc&&)
	{
	}
	catch(unique_winerror::exception&&)
	{
	}
	catch(unique_hresult::exception&&)
	{
	}
}

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
	l::wr::unique_close_window window;

	std::tie(winerror, window) = 
		l::wr::winerror_and_close_window(
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


#else

HINSTANCE g_hinst;

class Window
{
public:
 HWND GetHWND() { return m_hwnd; }
protected:
 virtual LRESULT HandleMessage(
                         UINT uMsg, WPARAM wParam, LPARAM lParam);
 virtual void PaintContent(PAINTSTRUCT *pps) { UNREFERENCED_PARAMETER(pps); }
 virtual LPCTSTR ClassName() = 0;
 virtual BOOL WinRegisterClass(WNDCLASS *pwc)
     { return RegisterClass(pwc); }
 virtual ~Window() { }

 HWND WinCreateWindow(DWORD dwExStyle, LPCTSTR pszName,
       DWORD dwStyle, int x, int y, int cx, int cy,
       HWND hwndParent, HMENU hmenu)
 {
  Register();
  return CreateWindowEx(dwExStyle, ClassName(), pszName, dwStyle,
                  x, y, cx, cy, hwndParent, hmenu, g_hinst, this);
 }
private:
 void Register();
 void OnPaint();
 void OnPrintClient(HDC hdc);
 static LRESULT CALLBACK s_WndProc(HWND hwnd,
     UINT uMsg, WPARAM wParam, LPARAM lParam);
protected:
 HWND m_hwnd;
};

void Window::Register()
{
    WNDCLASS wc;
    wc.style         = 0;
    wc.lpfnWndProc   = Window::s_WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = g_hinst;
    wc.hIcon         = NULL;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = ClassName();

    WinRegisterClass(&wc);
}

LRESULT CALLBACK Window::s_WndProc(
               HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
 Window *self;
 if (uMsg == WM_NCCREATE) {
  LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
  self = reinterpret_cast<Window *>(lpcs->lpCreateParams);
  self->m_hwnd = hwnd;
  SetWindowLongPtr(hwnd, GWLP_USERDATA,
            reinterpret_cast<LPARAM>(self));
 } else {
  self = reinterpret_cast<Window *>
            (GetWindowLongPtr(hwnd, GWLP_USERDATA));
 }
 if (self) {
  return self->HandleMessage(uMsg, wParam, lParam);
 } else {
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
 }
}

LRESULT Window::HandleMessage(
                          UINT uMsg, WPARAM wParam, LPARAM lParam)
{
 LRESULT lres;

 switch (uMsg) {
 case WM_NCDESTROY:
  lres = DefWindowProc(m_hwnd, uMsg, wParam, lParam);
  SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
  delete this;
  return lres;

 case WM_PAINT:
  OnPaint();
  return 0;

 case WM_PRINTCLIENT:
  OnPrintClient(reinterpret_cast<HDC>(wParam));
  return 0;
 }

 return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void Window::OnPaint()
{
 PAINTSTRUCT ps;
 BeginPaint(m_hwnd, &ps);
 PaintContent(&ps);
 EndPaint(m_hwnd, &ps);
}

void Window::OnPrintClient(HDC hdc)
{
 PAINTSTRUCT ps;
 ps.hdc = hdc;
 GetClientRect(m_hwnd, &ps.rcPaint);
 PaintContent(&ps);
}

class RootWindow : public Window
{
public:
 virtual LPCTSTR ClassName() { return TEXT("Scratch"); }
 static RootWindow *Create();
protected:
 LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
 LRESULT OnCreate();
private:
 HWND m_hwndChild;
};

LRESULT RootWindow::OnCreate()
{
 return 0;
}

LRESULT RootWindow::HandleMessage(
                          UINT uMsg, WPARAM wParam, LPARAM lParam)
{
 switch (uMsg) {
  case WM_CREATE:
   return OnCreate();  

  case WM_NCDESTROY:
   // Death of the root window ends the thread
   PostQuitMessage(0);
   break;

  case WM_SIZE:
   if (m_hwndChild) {
    SetWindowPos(m_hwndChild, NULL, 0, 0,
                 GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                 SWP_NOZORDER | SWP_NOACTIVATE);
   }
   return 0;

  case WM_SETFOCUS:
   if (m_hwndChild) {
    SetFocus(m_hwndChild);
   }
   return 0;
 }

 return __super::HandleMessage(uMsg, wParam, lParam);
}

RootWindow *RootWindow::Create()
{
 RootWindow *self = new RootWindow();
 if (self && self->WinCreateWindow(0,
       TEXT("Scratch"), WS_OVERLAPPEDWINDOW,
       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
       NULL, NULL)) {
      return self;
  }
 delete self;
 return NULL;
}


int PASCAL
wWinMain(HINSTANCE hinst, HINSTANCE, LPWSTR, int nShowCmd)
{
 g_hinst = hinst;

 if (SUCCEEDED(CoInitialize(NULL))) {
  InitCommonControls();

  RootWindow *prw = RootWindow::Create();
  if (prw) {
   ShowWindow(prw->GetHWND(), nShowCmd);
   MSG msg;
   while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
   }
  }
  CoUninitialize();
 }
 return 0;
}

#endif

