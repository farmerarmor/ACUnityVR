#include "GameDisplay.h"
#include "RenderSize.h"
#include <algorithm>
#include <atomic>
#include <cwchar>

namespace GameDisplay {
namespace {
HWND window{};WNDPROC original{};RECT previous{};
int outerWidth{},outerHeight{};
std::atomic<bool> active{};
BOOL CALLBACK Find(HWND candidate,LPARAM) {
    DWORD pid{};GetWindowThreadProcessId(candidate,&pid);
    if(pid!=GetCurrentProcessId() || !IsWindowVisible(candidate))return TRUE;
    wchar_t name[128]{};GetClassNameW(candidate,name,_countof(name));
    if(wcscmp(name,L"ScimitarEngineWindowClass"))return TRUE;
    if(window){window=nullptr;return FALSE;}window=candidate;return TRUE;
}
LRESULT CALLBACK Procedure(HWND w,UINT message,WPARAM wp,LPARAM lp) {
    auto result=CallWindowProcW(original,w,message,wp,lp);
    if(active && w==window && message==WM_GETMINMAXINFO) {
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMaxTrackSize.x=std::max<LONG>(limits->ptMaxTrackSize.x,outerWidth);
        limits->ptMaxTrackSize.y=std::max<LONG>(limits->ptMaxTrackSize.y,outerHeight);
        limits->ptMaxSize.x=std::max<LONG>(limits->ptMaxSize.x,outerWidth);
        limits->ptMaxSize.y=std::max<LONG>(limits->ptMaxSize.y,outerHeight);
    }
    return result;
}
}
bool Initialize(unsigned width,unsigned height) {
    if(!RenderSize::Valid(width,height) || original)return false;
    EnumWindows(Find,0);if(!window || !GetWindowRect(window,&previous))return false;
    RECT client{0,0,LONG(width),LONG(height)};
    auto style=DWORD(GetWindowLongPtrW(window,GWL_STYLE)),ex=DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE));
    if(!AdjustWindowRectEx(&client,style,GetMenu(window)!=nullptr,ex))return false;
    outerWidth=client.right-client.left;outerHeight=client.bottom-client.top;
    original=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC));if(!original)return false;
    active=true;
    SetLastError(0);
    auto previousProcedure=SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&Procedure));
    if(!previousProcedure && GetLastError()){active=false;original=nullptr;return false;}
    if(reinterpret_cast<WNDPROC>(previousProcedure)!=original){Stop();return false;}
    if(!SetWindowPos(window,nullptr,0,0,outerWidth,outerHeight,SWP_NOZORDER|SWP_NOACTIVATE)){Stop();return false;}
    RECT actual{};GetClientRect(window,&actual);
    if(unsigned(actual.right-actual.left)!=width || unsigned(actual.bottom-actual.top)!=height){Stop();return false;}
    return true;
}
void Stop() {
    active=false;
    if(window && original && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window,GWLP_WNDPROC))==Procedure) {
        SetWindowLongPtrW(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(original));
        SetWindowPos(window,nullptr,previous.left,previous.top,previous.right-previous.left,previous.bottom-previous.top,
            SWP_NOZORDER|SWP_NOACTIVATE);
    }
    // The dormant module and original procedure remain valid for queued calls.
}
}
