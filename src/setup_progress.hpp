#pragma once
#include "common.hpp"
#include <commctrl.h>
#include <thread>
#include <stdexcept>
#include <uxtheme.h>
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// The UI owns its own message loop so file copies, process waits and UAC
// approval never leave an unpainted/unresponsive installation window.
class SetupProgress {
    HWND window=nullptr,label=nullptr,bar=nullptr,button=nullptr,restartButton=nullptr;
    HANDLE ready=nullptr,dismissed=nullptr,action=nullptr;
    std::thread ui;
    HFONT headingFont=nullptr,bodyFont=nullptr;
    bool visible,completed=false,restartRequested=false;
    static constexpr UINT updateMessage=WM_APP+1,finishMessage=WM_APP+2,resultMessage=WM_APP+3;
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<SetupProgress*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<SetupProgress*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(self&&message==updateMessage){SetWindowTextW(self->label,reinterpret_cast<LPCWSTR>(l));SendMessageW(self->bar,PBM_SETPOS,w,0);return 0;}
        if(self&&message==resultMessage){bool success=(w&1)!=0,offer=(w&2)!=0;self->completed=true;self->restartRequested=false;SetWindowTextW(self->label,reinterpret_cast<LPCWSTR>(l));SendMessageW(self->bar,PBM_SETSTATE,success?PBST_NORMAL:PBST_ERROR,0);if(success)SendMessageW(self->bar,PBM_SETPOS,100,0);SetWindowTextW(self->button,offer?L"Later":L"Close");EnableWindow(self->button,TRUE);EnableWindow(self->restartButton,TRUE);ShowWindow(self->button,SW_SHOWNA);ShowWindow(self->restartButton,offer?SW_SHOWNA:SW_HIDE);EnableMenuItem(GetSystemMenu(hwnd,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_ENABLED);return 0;}
        if(message==WM_CTLCOLORSTATIC){SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);SetTextColor(reinterpret_cast<HDC>(w),RGB(32,49,77));return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));}
        if(message==finishMessage){DestroyWindow(hwnd);return 0;}
        if(message==WM_CLOSE){if(self&&self->completed)DestroyWindow(hwnd);return 0;}
        if(message==WM_COMMAND&&LOWORD(w)==IDOK){if(self&&self->completed)DestroyWindow(hwnd);return 0;}
        if(message==WM_COMMAND&&LOWORD(w)==IDYES){if(self&&self->completed&&(GetWindowLongW(self->restartButton,GWL_STYLE)&WS_VISIBLE)){self->restartRequested=true;self->completed=false;EnableWindow(self->button,FALSE);EnableWindow(self->restartButton,FALSE);EnableMenuItem(GetSystemMenu(hwnd,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_GRAYED);SetWindowTextW(self->label,L"Restarting Windows Explorer...");SetEvent(self->action);}return 0;}
        if(message==WM_DESTROY){if(self){SetEvent(self->dismissed);SetEvent(self->action);}PostQuitMessage(0);return 0;}
        return DefWindowProcW(hwnd,message,w,l);
    }
    void run(){
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_PROGRESS_CLASS};InitCommonControlsEx(&controls);
        auto instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=L"WindowsFolderSize.Setup.Progress";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&wc);
        RECT area{};SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);int width=560,height=345;
        window=CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"WindowsFolderSize Setup",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,(area.left+area.right-width)/2,(area.top+area.bottom-height)/2,width,height,nullptr,nullptr,instance,this);
        if(window){
            EnableMenuItem(GetSystemMenu(window,FALSE),SC_CLOSE,MF_BYCOMMAND|MF_GRAYED);
            HICON icon=LoadIconW(instance,MAKEINTRESOURCEW(1));SendMessageW(window,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(icon));SendMessageW(window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(icon));
            headingFont=CreateFontW(-25,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            bodyFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            HWND logo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_ICON,24,24,48,48,window,nullptr,instance,nullptr);SendMessageW(logo,STM_SETICON,reinterpret_cast<WPARAM>(icon),0);
            HWND heading=CreateWindowW(L"STATIC",L"WindowsFolderSize",WS_CHILD|WS_VISIBLE,86,23,420,34,window,nullptr,instance,nullptr);SendMessageW(heading,WM_SETFONT,reinterpret_cast<WPARAM>(headingFont),TRUE);
            HWND subtitle=CreateWindowW(L"STATIC",L"Folder sizes, right where you need them.   /   v1.0.0",WS_CHILD|WS_VISIBLE,86,61,420,24,window,nullptr,instance,nullptr);SendMessageW(subtitle,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
            label=CreateWindowW(L"STATIC",L"Preparing installation...",WS_CHILD|WS_VISIBLE,24,111,500,96,window,nullptr,instance,nullptr);
            bar=CreateWindowW(PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,24,220,500,12,window,nullptr,instance,nullptr);
            button=CreateWindowW(L"BUTTON",L"Close",WS_CHILD|WS_TABSTOP|BS_DEFPUSHBUTTON,414,261,110,34,window,reinterpret_cast<HMENU>(INT_PTR(IDOK)),instance,nullptr);
            restartButton=CreateWindowW(L"BUTTON",L"Restart Explorer",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,24,261,180,34,window,reinterpret_cast<HMENU>(INT_PTR(IDYES)),instance,nullptr);
            SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
            SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
            SendMessageW(restartButton,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
            SetWindowTheme(bar,L"",L"");SendMessageW(bar,PBM_SETBARCOLOR,0,RGB(18,91,232));SendMessageW(bar,PBM_SETBKCOLOR,0,RGB(232,239,250));SendMessageW(bar,PBM_SETRANGE32,0,100);
            if(visible){ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);}
        }
        SetEvent(ready);if(!window)return;
        MSG message;while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    }
public:
    explicit SetupProgress(bool show):visible(show){ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);dismissed=CreateEventW(nullptr,TRUE,FALSE,nullptr);action=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ready||!dismissed||!action){if(ready)CloseHandle(ready);if(dismissed)CloseHandle(dismissed);if(action)CloseHandle(action);throw std::runtime_error("cannot create setup progress event");}ui=std::thread([this]{run();});WaitForSingleObject(ready,INFINITE);if(!window||!label||!bar||!button||!restartButton){close();throw std::runtime_error("cannot create setup progress window");}}
    SetupProgress(const SetupProgress&)=delete;
    ~SetupProgress(){close();if(headingFont)DeleteObject(headingFont);if(bodyFont)DeleteObject(bodyFont);}
    void set(unsigned percent,const wchar_t* stage){auto text=std::to_wstring(percent)+L"%  ·  "+stage;if(window)SendMessageW(window,updateMessage,percent,reinterpret_cast<LPARAM>(text.c_str()));}
    bool result(const wchar_t* text,bool success,bool wait=true,bool offerRestart=false){ResetEvent(action);if(window)SendMessageW(window,resultMessage,(success?1:0)|(offerRestart?2:0),reinterpret_cast<LPARAM>(text));if(wait)WaitForSingleObject(action,INFINITE);return restartRequested;}
    void close(){if(ui.joinable()){if(IsWindow(window))SendMessageW(window,finishMessage,0,0);ui.join();window=nullptr;}if(ready){CloseHandle(ready);ready=nullptr;}if(dismissed){CloseHandle(dismissed);dismissed=nullptr;}if(action){CloseHandle(action);action=nullptr;}}
    bool verifyResult(const wchar_t* expected,bool success){wchar_t text[512];GetWindowTextW(label,text,512);return wcsstr(text,expected)&&(GetWindowLongW(button,GWL_STYLE)&WS_VISIBLE)&&SendMessageW(bar,PBM_GETSTATE,0,0)==(success?PBST_NORMAL:PBST_ERROR);}
    void savePreview(const wchar_t* path){
        RECT rect;GetWindowRect(window,&rect);int width=rect.right-rect.left,height=rect.bottom-rect.top;
        HDC source=GetDC(window),memory=CreateCompatibleDC(source);HBITMAP bitmap=CreateCompatibleBitmap(source,width,height);auto old=SelectObject(memory,bitmap);PatBlt(memory,0,0,width,height,WHITENESS);
        SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        std::vector<BYTE> pixels(width*height*4);SelectObject(memory,old);GetDIBits(memory,bitmap,0,height,pixels.data(),&info,DIB_RGB_COLORS);
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+DWORD(pixels.size());HANDLE out=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(out!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(out,&file,sizeof(file),&n,nullptr);WriteFile(out,&info.bmiHeader,sizeof(info.bmiHeader),&n,nullptr);WriteFile(out,pixels.data(),DWORD(pixels.size()),&n,nullptr);CloseHandle(out);}DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,source);
    }
    bool testDismiss(){SendMessageW(window,WM_COMMAND,IDOK,0);return WaitForSingleObject(dismissed,1000)==WAIT_OBJECT_0;}
    bool testRestartChoice(){SendMessageW(window,WM_COMMAND,IDYES,0);return WaitForSingleObject(action,1000)==WAIT_OBJECT_0&&restartRequested&&IsWindow(window);}
    bool verify(unsigned percent,const wchar_t* expected){wchar_t text[256];GetWindowTextW(label,text,256);return SendMessageW(bar,PBM_GETPOS,0,0)==percent&&wcsstr(text,expected)&&(IsWindowVisible(window)!=FALSE)==visible;}
};
