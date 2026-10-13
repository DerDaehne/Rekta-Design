/* platform_win32.c: Windows through Win32. Rekta's own title bar replaces the caption (WM_NCCALCSIZE), keeping the
   system's resize edges, snapping and shadow; pixels go up with SetDIBitsToDevice; DPI is per monitor; the theme
   comes from the registry (AppsUseLightTheme) and the motion setting from SPI_GETCLIENTAREAANIMATION. Functions
   newer than Windows 7 are looked up at run time, so the library links only against system DLLs. */
#include "rk_internal.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

typedef struct wwin { rk_window *w; HWND hwnd; bool own_frame; const rk_bitmap *last; float scale; int minw, minh; } wwin;

static HINSTANCE inst;
static DWORD main_thread;
static LARGE_INTEGER freq, t0;
static const wchar_t *CLASS = L"RektaWindow";
typedef UINT(WINAPI *GetDpiForWindow_t)(HWND);
typedef HRESULT(WINAPI *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
typedef struct { int l, r, t, b; } margins_t;
typedef HRESULT(WINAPI *DwmExtendFrame_t)(HWND, const margins_t *);
static GetDpiForWindow_t pGetDpiForWindow;
static DwmSetWindowAttribute_t pDwmSetWindowAttribute;
static DwmExtendFrame_t pDwmExtendFrame;

static double w_now(void) {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return (double)(t.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
}

static float dpi_of(HWND h) {
  UINT dpi = pGetDpiForWindow && h ? pGetDpiForWindow(h) : 0;
  if (!dpi) { HDC dc = GetDC(NULL); dpi = (UINT)GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(NULL, dc); }
  return dpi ? dpi / 96.0f : 1.0f;
}

static wchar_t *wide(const char *s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, NULL, 0);
  wchar_t *w = rk__alloc((size_t)n * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, w, n);
  return w;
}

static uint32_t mods_now(void) {
  return (GetKeyState(VK_SHIFT) < 0 ? RK_MOD_SHIFT : 0) | (GetKeyState(VK_CONTROL) < 0 ? RK_MOD_CTRL : 0) |
         (GetKeyState(VK_MENU) < 0 ? RK_MOD_ALT : 0) | ((GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) ? RK_MOD_SUPER : 0);
}

static uint32_t key_of(WPARAM vk) {
  switch (vk) {
    case VK_TAB: return RK_KEY_TAB; case VK_RETURN: return RK_KEY_ENTER; case VK_ESCAPE: return RK_KEY_ESCAPE; case VK_SPACE: return RK_KEY_SPACE;
    case VK_LEFT: return RK_KEY_LEFT; case VK_RIGHT: return RK_KEY_RIGHT; case VK_UP: return RK_KEY_UP; case VK_DOWN: return RK_KEY_DOWN;
    case VK_HOME: return RK_KEY_HOME; case VK_END: return RK_KEY_END; case VK_PRIOR: return RK_KEY_PAGE_UP; case VK_NEXT: return RK_KEY_PAGE_DOWN;
    case VK_BACK: return RK_KEY_BACKSPACE; case VK_DELETE: return RK_KEY_DELETE; case VK_BROWSER_BACK: return RK_KEY_BACK;
    default: break;
  }
  if (vk >= VK_F1 && vk <= VK_F10) return RK_KEY_F1 + (uint32_t)(vk - VK_F1);
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return (uint32_t)vk;
  return RK_KEY_NONE;
}

static void send(wwin *x, rk_event_type type, LPARAM lp, int button) {
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.type = type; e.time = w_now(); e.button = button; e.mods = mods_now();
  e.position = (rk_point){ GET_X_LPARAM(lp) / x->scale, GET_Y_LPARAM(lp) / x->scale };
  rk__window_event(x->w, &e);
}

static void blit(wwin *x, HDC dc) {
  const rk_bitmap *fb = x->last;
  if (!fb) return;
  BITMAPINFO bi;
  memset(&bi, 0, sizeof bi);
  bi.bmiHeader.biSize = sizeof bi.bmiHeader;
  bi.bmiHeader.biWidth = fb->stride; bi.bmiHeader.biHeight = -fb->h;   /* top-down */
  bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
  SetDIBitsToDevice(dc, 0, 0, (DWORD)fb->w, (DWORD)fb->h, 0, 0, 0, (UINT)fb->h, fb->px, &bi, DIB_RGB_COLORS);
}

static void sync_size(wwin *x) {
  RECT r;
  GetClientRect(x->hwnd, &r);
  rk__window_resized(x->w, (r.right - r.left) / x->scale, (r.bottom - r.top) / x->scale, x->scale);
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  wwin *x = (wwin *)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (!x) {
    if (msg == WM_NCCREATE) { x = ((CREATESTRUCTW *)lp)->lpCreateParams; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)x); x->hwnd = h; }
    return DefWindowProcW(h, msg, wp, lp);
  }
  switch (msg) {
    case WM_NCCALCSIZE:
      if (wp && x->own_frame) {
        /* the whole window is client area; maximized, it stays inside the monitor */
        if (IsZoomed(h)) {
          NCCALCSIZE_PARAMS *p = (NCCALCSIZE_PARAMS *)lp;
          int f = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
          p->rgrc[0].left += f; p->rgrc[0].top += f; p->rgrc[0].right -= f; p->rgrc[0].bottom -= f;
        }
        return 0;
      }
      break;
    case WM_NCHITTEST:
      if (x->own_frame && !IsZoomed(h)) {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        RECT r;
        ScreenToClient(h, &pt);
        GetClientRect(h, &r);
        int b = (int)(6 * x->scale);
        bool L = pt.x < b, R = pt.x >= r.right - b, T = pt.y < b, B = pt.y >= r.bottom - b;
        if (T) return L ? HTTOPLEFT : R ? HTTOPRIGHT : HTTOP;
        if (B) return L ? HTBOTTOMLEFT : R ? HTBOTTOMRIGHT : HTBOTTOM;
        if (L) return HTLEFT;
        if (R) return HTRIGHT;
        return HTCLIENT;
      }
      break;
    case WM_GETMINMAXINFO: {
      MINMAXINFO *m = (MINMAXINFO *)lp;
      m->ptMinTrackSize.x = x->minw; m->ptMinTrackSize.y = x->minh;
      return 0;
    }
    case WM_SIZE: sync_size(x); rk__window_frame(x->w, w_now()); return 0;
    case WM_DPICHANGED: {
      x->scale = HIWORD(wp) / 96.0f;
      RECT *r = (RECT *)lp;
      SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
      sync_size(x);
      return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      blit(x, dc);
      EndPaint(h, &ps);
      rk__window_invalidate(x->w);
      return 0;
    }
    case WM_CLOSE: rk__window_close_request(x->w); return 0;
    case WM_SETTINGCHANGE: case WM_THEMECHANGED: rk__app_settings_changed(); break;
    case WM_MOUSEMOVE: send(x, RK_EVENT_POINTER_MOVE, lp, 0); return 0;
    case WM_LBUTTONDOWN: SetCapture(h); send(x, RK_EVENT_POINTER_DOWN, lp, 0); return 0;
    case WM_LBUTTONUP: ReleaseCapture(); send(x, RK_EVENT_POINTER_UP, lp, 0); return 0;
    case WM_RBUTTONDOWN: send(x, RK_EVENT_POINTER_DOWN, lp, 1); return 0;
    case WM_RBUTTONUP: send(x, RK_EVENT_POINTER_UP, lp, 1); return 0;
    case WM_MBUTTONDOWN: send(x, RK_EVENT_POINTER_DOWN, lp, 2); return 0;
    case WM_MBUTTONUP: send(x, RK_EVENT_POINTER_UP, lp, 2); return 0;
    case WM_XBUTTONDOWN: {
      rk_event e;
      memset(&e, 0, sizeof e);
      e.size = sizeof e; e.type = RK_EVENT_KEY_DOWN; e.time = w_now(); e.key = RK_KEY_BACK;
      if (GET_XBUTTON_WPARAM(wp) == XBUTTON1) rk__window_event(x->w, &e);
      return TRUE;
    }
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {
      POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
      ScreenToClient(h, &pt);
      rk_event e;
      memset(&e, 0, sizeof e);
      e.size = sizeof e; e.type = RK_EVENT_WHEEL; e.time = w_now(); e.mods = mods_now();
      e.position = (rk_point){ pt.x / x->scale, pt.y / x->scale };
      float d = GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA * 48;
      if (msg == WM_MOUSEWHEEL) e.wheel_y = -d; else e.wheel_x = d;
      if ((e.mods & RK_MOD_SHIFT) && msg == WM_MOUSEWHEEL) { e.wheel_x = e.wheel_y; e.wheel_y = 0; }
      rk__window_event(x->w, &e);
      return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
      bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
      if (msg == WM_SYSKEYDOWN && wp == VK_F4) break;          /* Alt+F4 closes, as always */
      uint32_t k = key_of(wp);
      if (k) {
        rk_event e;
        memset(&e, 0, sizeof e);
        e.size = sizeof e; e.type = down ? RK_EVENT_KEY_DOWN : RK_EVENT_KEY_UP; e.time = w_now(); e.key = k; e.mods = mods_now();
        rk__window_event(x->w, &e);
      }
      if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) return 0;   /* no menu mode on Alt or F10 */
      break;
    }
    case WM_CHAR: {
      static wchar_t high;
      wchar_t c = (wchar_t)wp;
      if (c >= 0xD800 && c < 0xDC00) { high = c; return 0; }
      wchar_t s[3] = { c, 0, 0 };
      if (c >= 0xDC00 && c < 0xE000 && high) { s[0] = high; s[1] = c; high = 0; }
      if (s[0] < 0x20 || s[0] == 0x7f) return 0;
      uint32_t m = mods_now();
      if ((m & RK_MOD_CTRL) && !(m & RK_MOD_ALT)) return 0;     /* Ctrl shortcuts are keys, not text; AltGr is text */
      char u[16];
      int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, u, (int)sizeof u, NULL, NULL);
      if (n <= 0) return 0;
      rk_event e;
      memset(&e, 0, sizeof e);
      e.size = sizeof e; e.type = RK_EVENT_TEXT; e.time = w_now(); e.text = u; e.mods = m;
      rk__window_event(x->w, &e);
      return 0;
    }
    default: break;
  }
  return DefWindowProcW(h, msg, wp, lp);
}

static bool w_init(const rk_app_desc *d) {
  (void)d;
  inst = GetModuleHandleW(NULL);
  main_thread = GetCurrentThreadId();
  QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
  HMODULE user = GetModuleHandleW(L"user32.dll");
  typedef BOOL(WINAPI *SetDpiCtx_t)(HANDLE);
  SetDpiCtx_t setctx = user ? (SetDpiCtx_t)(void (*)(void))GetProcAddress(user, "SetProcessDpiAwarenessContext") : NULL;
  if (setctx) setctx((HANDLE)-4);                              /* per monitor, version 2 */
  else { typedef BOOL(WINAPI *SetAware_t)(void); SetAware_t aware = user ? (SetAware_t)(void (*)(void))GetProcAddress(user, "SetProcessDPIAware") : NULL; if (aware) aware(); }
  pGetDpiForWindow = user ? (GetDpiForWindow_t)(void (*)(void))GetProcAddress(user, "GetDpiForWindow") : NULL;
  HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
  if (dwm) {
    pDwmSetWindowAttribute = (DwmSetWindowAttribute_t)(void (*)(void))GetProcAddress(dwm, "DwmSetWindowAttribute");
    pDwmExtendFrame = (DwmExtendFrame_t)(void (*)(void))GetProcAddress(dwm, "DwmExtendFrameIntoClientArea");
  }
  WNDCLASSEXW wc;
  memset(&wc, 0, sizeof wc);
  wc.cbSize = sizeof wc; wc.style = CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = proc; wc.hInstance = inst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.lpszClassName = CLASS;
  wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
  return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

static void w_shutdown(void) { UnregisterClassW(CLASS, inst); }

static void dark_frame(wwin *x) {
  BOOL dark = rk__dark();
  if (pDwmSetWindowAttribute) pDwmSetWindowAttribute(x->hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
}

static void *w_open(rk_window *w, const rk_window_desc *d) {
  wwin *x = rk__alloc(sizeof *x);
  x->w = w; x->own_frame = !d->system_frame;
  float s = dpi_of(NULL);
  x->scale = s;
  RECT r = { 0, 0, (LONG)(d->width * s), (LONG)(d->height * s) };
  DWORD style = WS_OVERLAPPEDWINDOW;
  if (!x->own_frame) AdjustWindowRectEx(&r, style, FALSE, 0);
  x->minw = (int)(d->min_width * s); x->minh = (int)(d->min_height * s);
  wchar_t *title = wide(d->title);
  HWND h = CreateWindowExW(0, CLASS, title, style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, x);
  free(title);
  if (!h) { free(x); return NULL; }
  x->scale = dpi_of(h);
  if (x->own_frame && pDwmExtendFrame) { margins_t m = { 0, 0, 1, 0 }; pDwmExtendFrame(h, &m); }   /* keeps the shadow */
  dark_frame(x);
  SetWindowPos(h, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  sync_size(x);
  return x;
}

static void w_close(void *n) { wwin *x = n; if (x) { SetWindowLongPtrW(x->hwnd, GWLP_USERDATA, 0); DestroyWindow(x->hwnd); free(x); } }
static void w_show(void *n) { wwin *x = n; if (x) { ShowWindow(x->hwnd, SW_SHOW); UpdateWindow(x->hwnd); } }
static void w_title(void *n, const char *t) { wwin *x = n; if (x) { wchar_t *s = wide(t); SetWindowTextW(x->hwnd, s); free(s); } }
static void w_present(void *n, const rk_bitmap *fb, const rk_irect *changed, int nchanged) {
  (void)changed; (void)nchanged;                                               /* a local blit: the whole frame is cheap */
  wwin *x = n;
  if (!x) return;
  x->last = fb;
  HDC dc = GetDC(x->hwnd);
  blit(x, dc);
  ReleaseDC(x->hwnd, dc);
}
static void w_move(void *n) { wwin *x = n; if (x) { ReleaseCapture(); SendMessageW(x->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); } }
static void w_minimize(void *n) { wwin *x = n; if (x) ShowWindow(x->hwnd, SW_MINIMIZE); }
static void w_zoom(void *n) { wwin *x = n; if (x) ShowWindow(x->hwnd, IsZoomed(x->hwnd) ? SW_RESTORE : SW_MAXIMIZE); }

static bool w_wait(double ms) {
  MSG m;
  if (!PeekMessageW(&m, NULL, 0, 0, PM_NOREMOVE))
    MsgWaitForMultipleObjectsEx(0, NULL, ms < 0 ? INFINITE : (DWORD)ceil(ms), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
  while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
  return true;
}

static void w_wake(void) { PostThreadMessageW(main_thread, WM_NULL, 0, 0); }

static void w_settings(bool *dark, bool *reduced, bool *compact) {
  DWORD v = 1, size = sizeof v;
  *dark = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                       RRF_RT_REG_DWORD, NULL, &v, &size) == ERROR_SUCCESS && v == 0;
  BOOL anim = TRUE;
  *reduced = SystemParametersInfoW(0x1042 /* SPI_GETCLIENTAREAANIMATION */, 0, &anim, 0) && !anim;
  *compact = true;                                             /* a fine pointer: compact, as the web's media query */
}

static double w_refresh(void) {
  DEVMODEW dm;
  memset(&dm, 0, sizeof dm);
  dm.dmSize = sizeof dm;
  return EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1 ? (double)dm.dmDisplayFrequency : 60;
}

static void w_resize(void *n, float w, float h) {
  wwin *x = n;
  if (!x || IsZoomed(x->hwnd)) return;
  RECT r = { 0, 0, (LONG)lroundf(w * x->scale), (LONG)lroundf(h * x->scale) };
  if (!x->own_frame) AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
  SetWindowPos(x->hwnd, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

const rk_platform rk__platform_native = {
  "win32", w_init, w_shutdown, w_open, w_close, w_show, w_title, w_present, w_move, w_minimize, w_zoom,
  w_wait, w_wake, w_now, w_settings, w_resize, NULL, w_refresh, NULL
};
