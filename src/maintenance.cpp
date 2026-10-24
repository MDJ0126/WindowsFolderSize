#include "common.hpp"
#include "restart_policy.hpp"
#include <restartmanager.h>
using namespace fsizes;
void refreshUserRegistration(){
 const wchar_t* key=L"Software\\Classes\\CLSID\\{1A249399-E61D-4D3F-AC60-BEBBBDBBD115}\\InprocServer32";
 HKEY machine=nullptr,user=nullptr;wchar_t target[32768];DWORD bytes=sizeof(target),type=0;
 if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,key,0,KEY_QUERY_VALUE|KEY_WOW64_64KEY,&machine)!=ERROR_SUCCESS)return;
 auto error=RegQueryValueExW(machine,nullptr,nullptr,&type,reinterpret_cast<BYTE*>(target),&bytes);RegCloseKey(machine);
 if(error||type!=REG_SZ||bytes<2||bytes>sizeof(target))return;target[32767]=0;
 auto base=directory()+L"\\";if(_wcsnicmp(target,base.c_str(),base.size())||GetFileAttributesW(target)==INVALID_FILE_ATTRIBUTES)return;
 // An older per-user repair can shadow the new machine registration. Only
 // refresh our existing CLSID; normal machine installs create no HKCU class.
 if(RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_SET_VALUE|KEY_WOW64_64KEY,&user)==ERROR_SUCCESS){RegSetValueExW(user,nullptr,0,REG_SZ,reinterpret_cast<const BYTE*>(target),DWORD((wcslen(target)+1)*2));RegCloseKey(user);}
}
DWORD shellPid(){DWORD pid=0;GetWindowThreadProcessId(GetShellWindow(),&pid);return pid;}
bool launchExplorer(){
    wchar_t windows[32768];if(!GetWindowsDirectoryW(windows,32768))return false;
    auto exe=std::wstring(windows)+L"\\explorer.exe";auto command=L"\""+exe+L"\"";
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,windows,&si,&pi))return false;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);log(L"Explorer recovery launched");return true;
}
DWORD ensureDesktop(DWORD previous){return restoreDesktop(previous,shellPid,launchExplorer,[]{Sleep(200);});}
bool startGuard(DWORD previous){
    wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);
    auto command=L"\""+std::wstring(path)+L"\" --guard-shell "+std::to_wstring(previous);
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(path,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return false;
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;
}
DWORD restartExplorer(){
    DWORD pid=shellPid(),session=0,ownSession=0;if(!pid)return ensureDesktop(0);
    if(!ProcessIdToSessionId(pid,&session)||!ProcessIdToSessionId(GetCurrentProcessId(),&ownSession)||session!=ownSession)return ERROR_ACCESS_DENIED;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return GetLastError();
    wchar_t image[32768];DWORD n=32768;FILETIME created{},exited{},kernel{},user{};RM_UNIQUE_PROCESS app{};
    bool valid=QueryFullProcessImageNameW(process,0,image,&n)&&GetProcessTimes(process,&created,&exited,&kernel,&user);
    CloseHandle(process);auto name=valid?wcsrchr(image,L'\\'):nullptr;if(!name||_wcsicmp(name+1,L"explorer.exe"))return ERROR_INVALID_DATA;
    app.dwProcessId=pid;app.ProcessStartTime=created;DWORD handle=0;wchar_t key[CCH_RM_SESSION_KEY+1]{};
    DWORD error=RmStartSession(&handle,0,key);if(error)return error;
    error=RmRegisterResources(handle,0,nullptr,1,&app,0,nullptr);
    if(error){RmEndSession(handle);return error;}
    // Arm recovery before asking Explorer to exit. The guard survives this
    // helper exiting and never shuts down or kills any application itself.
    if(!startGuard(pid)){RmEndSession(handle);return GetLastError();}
    DWORD shutdown=RmShutdown(handle,0,nullptr);
    // Microsoft requires restart even if shutdown reports a partial failure.
    DWORD restarted=RmRestart(handle,0,nullptr);RmEndSession(handle);
    wchar_t message[160];swprintf_s(message,L"Explorer restart: shutdown=%lu restart=%lu",shutdown,restarted);log(message);
    // Crucially: do not return on RmRestart failure before restoring desktop.
    DWORD restored=ensureDesktop(pid);if(!restored)log(L"Explorer desktop restored and stable");return restored;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR command,int){
    if(wcsncmp(command,L"--guard-shell ",14)==0){
        DWORD previous=wcstoul(command+14,nullptr,10);if(!previous)return 2;
        for(unsigned i=0;i<300;++i){if(shellPid()!=previous){Sleep(700);return ensureDesktop(previous)?1:0;}Sleep(200);}return 0;
    }
    if(wcsncmp(command,L"--finish-install",16)==0){
        refreshUserRegistration();
        log(L"Installer Finish action entered");
        wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);auto broker=std::wstring(path);broker=broker.substr(0,broker.find_last_of(L'\\'))+L"\\WindowsFolderSize.exe";
        auto args=L"\""+broker+L"\"";STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
        if(!CreateProcessW(broker.c_str(),args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){log(L"Installer Finish could not start broker");return 3;}
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        const wchar_t* option=command+16;while(*option==L' ')++option;
        if(*option!=L'1'){log(L"Installer Finish: Explorer restart not selected");return 0;}
        log(L"Installer Finish: user selected Explorer restart");return restartExplorer()?1:0;
    }
    if(wcscmp(command,L"--restart-explorer"))return 2;
    DWORD error=restartExplorer();
    if(error){wchar_t text[256];swprintf_s(text,L"WindowsFolderSize is installed. File Explorer could not be restarted (error %lu). Open Task Manager, choose Run new task, and enter explorer.exe to restore the desktop.",error);MessageBoxW(nullptr,text,L"WindowsFolderSize",MB_OK|MB_ICONERROR|MB_SETFOREGROUND);}
    return error?1:0;
}
