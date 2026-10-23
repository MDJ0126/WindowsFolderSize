#include "common.hpp"
#include <restartmanager.h>
#include <shellapi.h>
using namespace fsizes;
DWORD restartExplorer(){
    DWORD pid=0,session=0,ownSession=0;GetWindowThreadProcessId(GetShellWindow(),&pid);
    if(!pid)return ERROR_NOT_FOUND;
    if(!ProcessIdToSessionId(pid,&session)||!ProcessIdToSessionId(GetCurrentProcessId(),&ownSession)||session!=ownSession)return ERROR_ACCESS_DENIED;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return GetLastError();
    wchar_t image[32768];DWORD n=32768;FILETIME created,exited,kernel,user;RM_UNIQUE_PROCESS app{};
    bool valid=QueryFullProcessImageNameW(process,0,image,&n)&&GetProcessTimes(process,&created,&exited,&kernel,&user);
    CloseHandle(process);auto name=valid?wcsrchr(image,L'\\'):nullptr;if(!name||_wcsicmp(name+1,L"explorer.exe"))return ERROR_INVALID_DATA;
    app.dwProcessId=pid;app.ProcessStartTime=created;DWORD handle=0;wchar_t key[CCH_RM_SESSION_KEY+1]{};
    DWORD error=RmStartSession(&handle,0,key);if(error)return error;
    error=RmRegisterResources(handle,0,nullptr,1,&app,0,nullptr);
    // No RmForceShutdown and no TerminateProcess. A shell that cannot close
    // normally is left for the user to restart at their next sign-in.
    if(!error)error=RmShutdown(handle,0,nullptr);
    if(!error)error=RmRestart(handle,0,nullptr);
    RmEndSession(handle);
    if(error)return error;
    for(unsigned i=0;i<30;i++){DWORD current=0;GetWindowThreadProcessId(GetShellWindow(),&current);if(current&&current!=pid)return ERROR_SUCCESS;Sleep(100);}
    wchar_t windows[32768];GetWindowsDirectoryW(windows,32768);auto exe=std::wstring(windows)+L"\\explorer.exe";
    SHELLEXECUTEINFOW launch{sizeof(launch)};launch.lpFile=exe.c_str();launch.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&launch))return GetLastError();
    for(unsigned i=0;i<100;i++){if(GetShellWindow())return ERROR_SUCCESS;Sleep(100);}
    return ERROR_TIMEOUT;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR command,int){
    if(wcscmp(command,L"--restart-explorer"))return 2;
    DWORD error=restartExplorer();
    if(error){wchar_t text[256];swprintf_s(text,L"Installation is complete, but File Explorer could not be restarted automatically (error %lu). Please sign out and sign in to apply WindowsFolderSize.",error);MessageBoxW(nullptr,text,L"WindowsFolderSize",MB_OK|MB_ICONINFORMATION);}
    return error?1:0;
}
