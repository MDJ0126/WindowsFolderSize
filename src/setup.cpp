#include "common.hpp"
#include <shellapi.h>
#include <tlhelp32.h>
#include <filesystem>
#include <array>
#include <memory>
#include <shlobj.h>
#include "setup_progress.hpp"
using namespace fsizes;
namespace f=std::filesystem;
const wchar_t* uninstallKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WindowsFolderSize";
const wchar_t* extensionGuid=L"{1A249399-E61D-4D3F-AC60-BEBBBDBBD115}";
const wchar_t* overlayKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ShellIconOverlayIdentifiers\\WindowsFolderSize";
const wchar_t* contextKey=L"Software\\Classes\\Folder\\shellex\\ContextMenuHandlers\\WindowsFolderSize";
std::wstring installationRoot(){PWSTR path=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path)))throw std::runtime_error("Cannot locate Local AppData.");std::wstring root=std::wstring(path)+L"\\WindowsFolderSize";CoTaskMemFree(path);return root;}
void registryString(HKEY key,const wchar_t* name,const std::wstring& value){if(RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)))!=ERROR_SUCCESS)throw std::runtime_error("registry write failed");}
void launch(const std::wstring& exe,const std::wstring& args,bool wait=false){std::wstring cmd=L"\""+exe+L"\" "+args;STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))throw std::runtime_error("process launch failed");if(wait)WaitForSingleObject(pi.hProcess,10000);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
void stopAgent(const std::wstring& root){
    if(f::exists(root+L"\\WindowsFolderSize.exe"))launch(root+L"\\WindowsFolderSize.exe",L"--stop",true);
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("cannot inspect running WindowsFolderSize process");
    PROCESSENTRY32W p{sizeof(p)};bool stopped=true;
    if(Process32FirstW(snapshot,&p))do{
        if(_wcsicmp(p.szExeFile,L"WindowsFolderSize.exe"))continue;
        HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,p.th32ProcessID);if(!h)continue;
        wchar_t image[32768];DWORD n=32768;
        if(QueryFullProcessImageNameW(h,0,image,&n)&&_wcsicmp(image,(root+L"\\WindowsFolderSize.exe").c_str())==0){
            if(WaitForSingleObject(h,3000)==WAIT_TIMEOUT){
                // TerminateProcess is asynchronous: the executable remains
                // locked until the process handle becomes signalled.
                if(!TerminateProcess(h,0)||WaitForSingleObject(h,10000)!=WAIT_OBJECT_0)stopped=false;
            }
        }CloseHandle(h);
    }while(Process32NextW(snapshot,&p));CloseHandle(snapshot);
    if(!stopped)throw std::runtime_error("WindowsFolderSize did not stop; installation files were not replaced");
}
bool samePayload(int id,const std::wstring& path){HRSRC res=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);if(!res)return false;DWORD length=SizeofResource(nullptr,res);auto data=static_cast<const BYTE*>(LockResource(LoadResource(nullptr,res)));HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;LARGE_INTEGER size{};bool same=GetFileSizeEx(file,&size)&&size.QuadPart==length;BYTE block[32768];DWORD offset=0;while(same&&offset<length){DWORD read=0,want=(std::min)(DWORD(sizeof(block)),length-offset);same=ReadFile(file,block,want,&read,nullptr)&&read==want&&memcmp(block,data+offset,want)==0;offset+=want;}CloseHandle(file);return same;}
std::runtime_error fileError(const std::wstring& path,DWORD error){int n=WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,nullptr,0,nullptr,nullptr);std::string text(n,'\0');WideCharToMultiByte(CP_UTF8,0,path.c_str(),-1,text.data(),n,nullptr,nullptr);text.pop_back();return std::runtime_error("Cannot write: "+text+" (Windows error "+std::to_string(error)+")");}
void extract(int id,const std::wstring& path){if(samePayload(id,path))return;HRSRC res=FindResourceW(nullptr,MAKEINTRESOURCEW(id),RT_RCDATA);if(!res)throw std::runtime_error("payload missing");DWORD length=SizeofResource(nullptr,res);const void* data=LockResource(LoadResource(nullptr,res));HANDLE out=INVALID_HANDLE_VALUE;DWORD error=0;
    auto deadline=GetTickCount64()+5000;
    do{out=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(out!=INVALID_HANDLE_VALUE)break;error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION)break;Sleep(100);}while(GetTickCount64()<deadline);
    if(out==INVALID_HANDLE_VALUE)throw fileError(path,error);DWORD written;bool ok=WriteFile(out,data,length,&written,nullptr)&&written==length;error=GetLastError();CloseHandle(out);if(!ok)throw fileError(path,error);}
std::wstring hookName(const std::wstring& root){wchar_t name[256];GetPrivateProfileStringW(L"Install",L"Hook",L"WindowsFolderSizeHook.dll",name,256,(root+L"\\install.ini").c_str());std::wstring result(name);if(result.find_first_of(L"\\/:")!=std::wstring::npos)throw std::runtime_error("invalid hook file name");return result;}
std::wstring selectHook(const std::wstring& root){auto current=hookName(root);if(samePayload(102,root+L"\\"+current))return current;HRSRC res=FindResourceW(nullptr,MAKEINTRESOURCEW(102),RT_RCDATA);auto data=static_cast<const BYTE*>(LockResource(LoadResource(nullptr,res)));DWORD count=SizeofResource(nullptr,res);uint64_t hash=14695981039346656037ull;for(DWORD i=0;i<count;i++){hash^=data[i];hash*=1099511628211ull;}wchar_t name[80];swprintf_s(name,L"WindowsFolderSizeHook-%016llx.dll",hash);auto path=root+L"\\"+name;if(f::exists(path)&&!samePayload(102,path))throw std::runtime_error("hook version name collision");return name;}
bool registered(const std::wstring& root,const std::wstring& hook){wchar_t value[32768];DWORD size=sizeof(value);auto key=L"Software\\Classes\\CLSID\\"+std::wstring(extensionGuid)+L"\\InprocServer32";if(RegGetValueW(HKEY_LOCAL_MACHINE,key.c_str(),nullptr,RRF_RT_REG_SZ,nullptr,value,&size)!=ERROR_SUCCESS||_wcsicmp(value,(root+L"\\"+hook).c_str())!=0)return false;size=sizeof(value);if(RegGetValueW(HKEY_LOCAL_MACHINE,contextKey,nullptr,RRF_RT_REG_SZ,nullptr,value,&size)!=ERROR_SUCCESS||wcscmp(value,extensionGuid)!=0)return false;size=sizeof(value);return RegGetValueW(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",extensionGuid,RRF_RT_REG_SZ,nullptr,value,&size)==ERROR_SUCCESS;}
bool checkedRoot(const std::wstring& root){auto expected=installationRoot();if(_wcsicmp(root.c_str(),expected.c_str())!=0)return false;DWORD attrs=GetFileAttributesW(root.c_str());return attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_REPARSE_POINT);}
bool registration(const std::wstring& setup,const wchar_t* operation){SHELLEXECUTEINFOW info{sizeof(info)};info.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_FLAG_NO_UI;info.lpVerb=L"runas";info.lpFile=setup.c_str();info.lpParameters=operation;info.nShow=SW_HIDE;if(!ShellExecuteExW(&info))return false;WaitForSingleObject(info.hProcess,120000);DWORD code=1;GetExitCodeProcess(info.hProcess,&code);CloseHandle(info.hProcess);return code==0;}
void restartExplorer(){
    DWORD pid=0;GetWindowThreadProcessId(GetShellWindow(),&pid);
    if(pid){
        DWORD shellSession=0,ownSession=0;ProcessIdToSessionId(pid,&shellSession);ProcessIdToSessionId(GetCurrentProcessId(),&ownSession);
        if(shellSession!=ownSession)throw std::runtime_error("Explorer belongs to another session; restart was cancelled.");
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,pid);
        if(!process)throw std::runtime_error("Cannot open Windows Explorer for restart.");
        wchar_t image[32768];DWORD length=32768;bool valid=QueryFullProcessImageNameW(process,0,image,&length)!=FALSE;
        auto name=valid?wcsrchr(image,L'\\'):nullptr;valid=name&&_wcsicmp(name+1,L"explorer.exe")==0;
        bool stopped=valid&&TerminateProcess(process,0)&&WaitForSingleObject(process,10000)==WAIT_OBJECT_0;CloseHandle(process);
        if(!stopped)throw std::runtime_error("Windows Explorer could not be stopped. Installation is complete; restart Explorer manually.");
    }
    // Windows may recover its shell automatically. Avoid opening an extra
    // Explorer window if recovery has already created a new desktop shell.
    for(unsigned i=0;i<20;i++){DWORD current=0;GetWindowThreadProcessId(GetShellWindow(),&current);if(current&&current!=pid)return;Sleep(100);}
    wchar_t windows[32768];GetWindowsDirectoryW(windows,32768);launch(std::wstring(windows)+L"\\explorer.exe",L"");
    for(unsigned i=0;i<100;i++){if(GetShellWindow())return;Sleep(100);}
    throw std::runtime_error("Explorer was launched but its desktop is not ready yet. Installation is complete.");
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int argc;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);bool uninstall=false,cleanup=false,silent=false,registerOnly=false,unregisterOnly=false,noElevation=false;for(int i=1;i<argc;i++){if(wcscmp(argv[i],L"--uninstall")==0)uninstall=true;if(wcscmp(argv[i],L"--cleanup")==0)cleanup=true;if(wcscmp(argv[i],L"--silent")==0)silent=true;if(wcscmp(argv[i],L"--register-extension")==0)registerOnly=true;if(wcscmp(argv[i],L"--unregister-extension")==0)unregisterOnly=true;if(wcscmp(argv[i],L"--no-elevation")==0)noElevation=true;}LocalFree(argv);
    std::unique_ptr<SetupProgress> progress;
    auto stage=[&](unsigned percent,const wchar_t* text){if(progress)progress->set(percent,text);};
    try{
        if(wcsstr(GetCommandLineW(),L"--self-test")){
            SetupProgress hidden(false);hidden.set(5,L"Preparing");if(!hidden.verify(5,L"Preparing"))return 41;
            hidden.set(65,L"Copying files");if(!hidden.verify(65,L"Copying files"))return 42;
            hidden.set(100,L"Complete");if(!hidden.verify(100,L"Complete"))return 43;
            hidden.result(L"Installation failure test",false,false);if(!hidden.verifyResult(L"Installation failure test",false))return 46;
            hidden.result(L"Installation complete test",true,false);if(!hidden.verifyResult(L"Installation complete test",true)||!hidden.verify(100,L"Installation complete test"))return 47;
            hidden.result(L"Installation complete - WindowsFolderSize 1.0.0\nRestart Windows Explorer now to apply the changes?\nOpen File Explorer windows will close. Choose Later to apply on your next sign-in.",true,false,true);wchar_t preview[32768];if(GetEnvironmentVariableW(L"WFS_PREVIEW_PATH",preview,32768))hidden.savePreview(preview);if(!hidden.testRestartChoice())return 49;
            hidden.result(L"Explorer restarted.",true,false);
            if(!hidden.testDismiss())return 48;
            {SetupProgress later(false);later.result(L"Restart Windows Explorer now?",true,false,true);if(!later.testDismiss())return 50;}
            wchar_t temp[32768];GetTempPathW(32768,temp);auto testRoot=std::wstring(temp)+L"WindowsFolderSize-setup-test-"+std::to_wstring(GetCurrentProcessId());f::create_directory(testRoot);auto file=testRoot+L"\\locked-payload.bin";
            HANDLE locked=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(locked==INVALID_HANDLE_VALUE)return 44;
            std::thread release([locked]{Sleep(500);CloseHandle(locked);});
            try{extract(101,file);}catch(...){release.join();throw;}release.join();
            bool match=samePayload(101,file);f::remove(file);f::remove(testRoot);return match?0:45;
        }
        auto root=installationRoot();
        if(registerOnly||unregisterOnly){
            std::wstring clsidKey=L"Software\\Classes\\CLSID\\"+std::wstring(extensionGuid);
            if(unregisterOnly){RegDeleteTreeW(HKEY_LOCAL_MACHINE,overlayKey);RegDeleteTreeW(HKEY_LOCAL_MACHINE,clsidKey.c_str());RegDeleteTreeW(HKEY_CURRENT_USER,clsidKey.c_str());RegDeleteTreeW(HKEY_CURRENT_USER,contextKey);RegDeleteTreeW(HKEY_LOCAL_MACHINE,contextKey);HKEY approved;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",0,KEY_SET_VALUE,&approved)==ERROR_SUCCESS){RegDeleteValueW(approved,extensionGuid);RegCloseKey(approved);}return 0;}
            RegDeleteTreeW(HKEY_CURRENT_USER,clsidKey.c_str());RegDeleteTreeW(HKEY_CURRENT_USER,contextKey);
            HKEY clsid;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,(clsidKey+L"\\InprocServer32").c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&clsid,nullptr)!=ERROR_SUCCESS)return 7;registryString(clsid,nullptr,root+L"\\"+hookName(root));registryString(clsid,L"ThreadingModel",L"Apartment");RegCloseKey(clsid);
            HKEY overlay;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,overlayKey,0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&overlay,nullptr)!=ERROR_SUCCESS)return 8;registryString(overlay,nullptr,extensionGuid);RegCloseKey(overlay);
            HKEY context;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,contextKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&context,nullptr)!=ERROR_SUCCESS)return 9;registryString(context,nullptr,extensionGuid);RegCloseKey(context);
            HKEY approved;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",0,nullptr,0,KEY_SET_VALUE,nullptr,&approved,nullptr)!=ERROR_SUCCESS)return 10;registryString(approved,extensionGuid,L"WindowsFolderSize");RegCloseKey(approved);return 0;
        }
        if(cleanup){if(!checkedRoot(root))return 3;std::error_code ec;f::remove_all(root,ec);return ec?4:0;}
        if(uninstall){
            if(!silent&&MessageBoxW(nullptr,L"Remove WindowsFolderSize? Folder sizes will stop displaying. Files still in use will be removed at your next sign-in.",L"Uninstall WindowsFolderSize",MB_YESNO|MB_ICONQUESTION)!=IDYES)return 0;
            if(!registration(root+L"\\WindowsFolderSizeSetup.exe",L"--unregister-extension"))throw std::runtime_error("extension removal not approved");
            stopAgent(root);HKEY run;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&run)==ERROR_SUCCESS){RegDeleteValueW(run,L"WindowsFolderSize");RegCloseKey(run);}RegDeleteTreeW(HKEY_CURRENT_USER,uninstallKey);
            wchar_t temp[32768],self[32768];GetTempPathW(32768,temp);GetModuleFileNameW(nullptr,self,32768);auto helper=std::wstring(temp)+L"WindowsFolderSizeCleanup-"+std::to_wstring(GetCurrentProcessId())+L".exe";if(!CopyFileW(self,helper.c_str(),FALSE))throw std::runtime_error("cannot prepare cleanup");
            HKEY once;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",0,nullptr,0,KEY_SET_VALUE,nullptr,&once,nullptr)!=ERROR_SUCCESS)throw std::runtime_error("cannot schedule cleanup");registryString(once,L"WindowsFolderSizeCleanup",L"\""+helper+L"\" --cleanup");RegCloseKey(once);
            if(!silent)MessageBoxW(nullptr,L"WindowsFolderSize has stopped and automatic startup is disabled. Remaining files will be removed at your next sign-in.",L"WindowsFolderSize",MB_OK|MB_ICONINFORMATION);return 0;
        }
        HANDLE setupMutex=CreateMutexW(nullptr,FALSE,L"Local\\WindowsFolderSize.Setup.Install");DWORD mutexError=GetLastError();
        if(!setupMutex)throw std::runtime_error("cannot create installation lock");
        struct MutexHandle{HANDLE value;~MutexHandle(){CloseHandle(value);}} setupLock{setupMutex};
        if(mutexError==ERROR_ALREADY_EXISTS)throw std::runtime_error("Another WindowsFolderSize installer is already running. Please wait for it to finish.");
        if(!silent)progress=std::make_unique<SetupProgress>(true);
        stage(5,L"Checking installation files");
        if(!samePayload(101,root+L"\\WindowsFolderSize.exe")){stage(10,L"Waiting for WindowsFolderSize to stop");stopAgent(root);}f::create_directories(root+L"\\Symbols");
        stage(20,L"Copying Explorer extension");
        auto selectedHook=selectHook(root);extract(102,root+L"\\"+selectedHook);
        const std::array<std::pair<int,const wchar_t*>,8> files={{{101,L"WindowsFolderSize.exe"},{103,L"Symbols\\WindowsFolderSizeSymbols.exe"},{104,L"Symbols\\dbghelp.dll"},{105,L"Symbols\\symsrv.dll"},{106,L"LICENSE.txt"},{107,L"THIRD_PARTY.md"},{108,L"MinHook-LICENSE.txt"},{109,L"WindowsFolderSize.ico"}}};unsigned copied=0;for(auto& item:files){auto message=L"Copying: "+std::wstring(item.second);stage(25+copied*6,message.c_str());extract(item.first,root+L"\\"+item.second);++copied;}
        WritePrivateProfileStringW(L"Install",L"Hook",selectedHook.c_str(),(root+L"\\install.ini").c_str());
        wchar_t self[32768];GetModuleFileNameW(nullptr,self,32768);auto installer=root+L"\\WindowsFolderSizeSetup.exe";if(_wcsicmp(self,installer.c_str())!=0&&!CopyFileW(self,installer.c_str(),FALSE))throw std::runtime_error("cannot save uninstaller");
        stage(80,L"Registering Explorer extension - waiting for approval");
        if(!registered(root,selectedHook)&&(noElevation||!registration(installer,L"--register-extension")))throw std::runtime_error("Explorer extension registration requires approval; no changes to security settings were made");
        stage(90,L"Configuring automatic startup");
        HKEY run;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&run,nullptr)!=ERROR_SUCCESS)throw std::runtime_error("cannot register startup");registryString(run,L"WindowsFolderSize",L"\""+root+L"\\WindowsFolderSize.exe\"");RegCloseKey(run);
        HKEY entry;if(RegCreateKeyExW(HKEY_CURRENT_USER,uninstallKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&entry,nullptr)!=ERROR_SUCCESS)throw std::runtime_error("cannot register uninstaller");registryString(entry,L"DisplayName",L"WindowsFolderSize");registryString(entry,L"DisplayVersion",L"1.0.0");registryString(entry,L"Publisher",L"WindowsFolderSize project");registryString(entry,L"InstallLocation",root);registryString(entry,L"DisplayIcon",installer);registryString(entry,L"UninstallString",L"\""+installer+L"\" --uninstall");registryString(entry,L"QuietUninstallString",L"\""+installer+L"\" --uninstall --silent");DWORD one=1;RegSetValueExW(entry,L"NoModify",0,REG_DWORD,reinterpret_cast<BYTE*>(&one),sizeof(one));RegSetValueExW(entry,L"NoRepair",0,REG_DWORD,reinterpret_cast<BYTE*>(&one),sizeof(one));RegCloseKey(entry);
        stage(95,L"Starting background service");launch(root+L"\\WindowsFolderSize.exe",L"");stage(100,L"Installation complete");
        if(progress&&progress->result(L"Installation complete - WindowsFolderSize 1.0.0\nRestart Windows Explorer now to apply the changes?\nOpen File Explorer windows will close. Choose Later to apply on your next sign-in.",true,true,true)){
            restartExplorer();progress->result(L"Installation complete.\nWindows Explorer has restarted. WindowsFolderSize is ready.\nWindowsFolderSize starts automatically when you sign in.",true);
        }return 0;
    }catch(const std::exception& e){std::string text=e.what();int count=MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,nullptr,0);std::wstring detail(count,L'\0');if(count>0){MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,detail.data(),count);log((L"Setup failed: "+detail).c_str());}else log(L"Setup failed");if(progress)progress->result((L"Installation failed\n"+detail).c_str(),false);else if(!silent)MessageBoxA(nullptr,text.c_str(),"WindowsFolderSize setup failed",MB_OK|MB_ICONERROR);return 1;}
}
