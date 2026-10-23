#include "common.hpp"
#include "scanner.hpp"
#include <shlobj.h>
#include <sddl.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <thread>
#include <set>
#include <fstream>
using namespace fsizes;
std::atomic<bool> stopping=false;
struct Watch {
    HANDLE directory=INVALID_HANDLE_VALUE; OVERLAPPED ov{}; BYTE buffer[4096]{};
    bool arm(){ResetEvent(ov.hEvent);return ReadDirectoryChangesW(directory,buffer,sizeof(buffer),TRUE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE,nullptr,&ov,nullptr)!=FALSE;}
    Watch(const std::wstring& path){
        directory=CreateFileW(longPath(path).c_str(),FILE_LIST_DIRECTORY,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr);
        if(directory!=INVALID_HANDLE_VALUE){ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ov.hEvent||!arm()){CloseHandle(directory);directory=INVALID_HANDLE_VALUE;}}
    }
    ~Watch(){if(directory!=INVALID_HANDLE_VALUE){CancelIoEx(directory,&ov);DWORD ignored;GetOverlappedResult(directory,&ov,&ignored,TRUE);CloseHandle(directory);}if(ov.hEvent)CloseHandle(ov.hEvent);}
    bool changed(){if(directory==INVALID_HANDLE_VALUE||WaitForSingleObject(ov.hEvent,0)!=WAIT_OBJECT_0)return false;DWORD n;GetOverlappedResult(directory,&ov,&n,FALSE);arm();return true;}
};
struct Entry {std::wstring path;uint64_t bytes=0;ULONGLONG used=0,finished=0,lastChange=0,lastAnimation=0;unsigned percent=0;DWORD error=0;bool ready=false,queued=false,running=false,changed=false;std::unique_ptr<Watch> watch;};
std::mutex cacheMutex;std::condition_variable cv;std::map<std::wstring,Entry> cache;std::deque<std::wstring> pending;
std::wstring normalized(std::wstring p){while(p.size()>3&&p.back()==L'\\')p.pop_back();CharLowerBuffW(p.data(),DWORD(p.size()));return p;}
bool acceptable(const std::wstring& p){return p.size()>=3&&p.size()<32000&&iswalpha(p[0])&&p[1]==L':'&&p[2]==L'\\'&&p.find(L'\0')==std::wstring::npos;}
void queue(const std::wstring& key,Entry& e){if(!e.queued&&!e.running){e.queued=true;e.percent=0;e.error=0;pending.push_back(key);cv.notify_one();}}
void notify(const std::wstring& path){SHChangeNotify(SHCNE_UPDATEITEM,SHCNF_PATHW|SHCNF_FLUSHNOWAIT,path.c_str(),nullptr);auto parent=path.substr(0,path.find_last_of(L'\\'));if(parent.size()==2)parent+=L'\\';SHChangeNotify(SHCNE_UPDATEDIR,SHCNF_PATHW|SHCNF_FLUSHNOWAIT,parent.c_str(),nullptr);}
void scannerWorker(){
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    struct Running {std::wstring key,path;std::unique_ptr<ScanJob> scan;ULONGLONG lastNotify=0;unsigned lastPercent=101;};
    std::deque<std::unique_ptr<Running>> active;
    while(!stopping){
        // Admit all newly requested folders before the next short work slice.
        std::vector<std::pair<std::wstring,std::wstring>> incoming;
        {std::unique_lock lock(cacheMutex);if(active.empty())cv.wait(lock,[]{return stopping||!pending.empty();});if(stopping)break;
            while(!pending.empty()){auto key=std::move(pending.front());pending.pop_front();auto it=cache.find(key);if(it==cache.end())continue;auto& e=it->second;e.queued=false;e.running=true;e.changed=false;incoming.emplace_back(key,e.path);}}
        for(auto& request:incoming){
            auto job=std::make_unique<Running>();job->key=std::move(request.first);job->path=std::move(request.second);auto ptr=job.get();
            job->scan=std::make_unique<ScanJob>(job->path,stopping,[ptr](unsigned percent,uint64_t){
                {std::lock_guard lock(cacheMutex);auto it=cache.find(ptr->key);if(it!=cache.end())it->second.percent=(std::min)(percent,99u);}
                auto now=GetTickCount64();if(percent!=ptr->lastPercent&&(now-ptr->lastNotify>=200||ptr->lastNotify==0)){notify(ptr->path);ptr->lastNotify=now;ptr->lastPercent=percent;}
            });active.push_back(std::move(job));
        }
        if(active.empty())continue;auto job=std::move(active.front());active.pop_front();
        if(!job->scan->step()){active.push_back(std::move(job));continue;}
        auto result=job->scan->value();
        {std::lock_guard lock(cacheMutex);auto it=cache.find(job->key);if(it==cache.end())continue;auto& e=it->second;e.running=false;e.finished=GetTickCount64();e.error=result.error;
            // Retain the last completed snapshot during background refreshes.
            // Changes during enumeration request another pass after debounce;
            // they must not throw away a successfully calculated size.
            if(result.complete){e.bytes=result.bytes;e.ready=true;e.percent=100;}}
        notify(job->path);
        if(!result.complete){wchar_t message[200];swprintf_s(message,L"Folder calculation unavailable (Win32 error %lu)",result.error);log(message);}
    }
}
void pipeWorker(){
    auto name=sessionPipe();
    // Only the current user and SYSTEM can connect to this local pipe.
    HANDLE token;std::vector<BYTE> tokenData;std::wstring sddl=L"D:P(A;;GA;;;SY)";
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){DWORD n=0;GetTokenInformation(token,TokenUser,nullptr,0,&n);tokenData.resize(n);if(GetTokenInformation(token,TokenUser,tokenData.data(),n,&n)){LPWSTR sid=nullptr;ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(tokenData.data())->User.Sid,&sid);if(sid){sddl+=L"(A;;GA;;;"+std::wstring(sid)+L")";LocalFree(sid);}}CloseHandle(token);}
    PSECURITY_DESCRIPTOR sd=nullptr;ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr);SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
    HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,64,65536,50,&sa);if(sd)LocalFree(sd);
    if(pipe==INVALID_HANDLE_VALUE){log(L"Cannot create local size cache pipe");stopping=true;cv.notify_all();return;}
    while(!stopping){
        BOOL connected=ConnectNamedPipe(pipe,nullptr);if(!connected&&GetLastError()!=ERROR_PIPE_CONNECTED)continue;
        Request req{};DWORD n=0;Response response{};response.state=ScanState::failed;
        std::vector<BYTE> data(sizeof(Request)+32000*sizeof(wchar_t));
        if(ReadFile(pipe,data.data(),DWORD(data.size()),&n,nullptr)&&n>=sizeof(req)){
            memcpy(&req,data.data(),sizeof(req));
            if((req.magicValue==magic||req.magicValue==progressMagic)&&req.characters>0&&req.characters<32000&&n==sizeof(req)+req.characters*sizeof(wchar_t)){
                std::wstring path(reinterpret_cast<wchar_t*>(data.data()+sizeof(req)),req.characters);
                if(acceptable(path)){
                    auto key=normalized(path);std::lock_guard lock(cacheMutex);auto it=cache.find(key);
                    if(it==cache.end()&&cache.size()<256){Entry entry;entry.path=path;entry.used=GetTickCount64();entry.watch=std::make_unique<Watch>(path);it=cache.emplace(key,std::move(entry)).first;queue(key,it->second);}
                    if(it!=cache.end()){auto& e=it->second;e.used=GetTickCount64();response.ready=e.ready;response.bytes=e.bytes;response.percent=e.percent;response.state=e.ready?ScanState::complete:e.error?ScanState::failed:e.running?ScanState::scanning:ScanState::queued;if(!e.ready&&!e.queued&&!e.running&&e.used-e.finished>10000)queue(key,e);}
                }
            }
        }
        if(req.magicValue==magic){LegacyResponse legacy;legacy.ready=response.ready;legacy.bytes=response.bytes;WriteFile(pipe,&legacy,sizeof(legacy),&n,nullptr);}else WriteFile(pipe,&response,sizeof(response),&n,nullptr);FlushFileBuffers(pipe);DisconnectNamedPipe(pipe);
    }CloseHandle(pipe);
}
bool symbolsValid(){DWORD stamp,size;if(!imageIdentity(storagePath(),stamp,size))return false;auto ini=dataDirectory()+L"\\hooks.ini";return GetPrivateProfileIntW(L"Symbols",L"TimeStamp",0,ini.c_str())==stamp&&GetPrivateProfileIntW(L"Symbols",L"ImageSize",0,ini.c_str())==size&&GetPrivateProfileIntW(L"Symbols",L"GetSize",0,ini.c_str())&&GetPrivateProfileIntW(L"Symbols",L"GetItemProperty",0,ini.c_str())&&GetPrivateProfileIntW(L"Symbols",L"ItemGetValue",0,ini.c_str());}
bool resolveSymbols(){if(symbolsValid())return true;log(L"Preparing symbols for this Windows version");auto exe=directory()+L"\\Symbols\\WindowsFolderSizeSymbols.exe";std::wstring cmd=L"\""+exe+L"\"";STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return false;WaitForSingleObject(pi.hProcess,180000);DWORD code;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code==0&&symbolsValid();}
int wmain(int argc,wchar_t**argv){
    if(argc>2&&wcscmp(argv[1],L"--scan")==0){auto r=scan(argv[2],stopping);printf("{\"complete\":%s,\"bytes\":%llu,\"files\":%llu,\"directories\":%llu,\"error\":%lu}\n",r.complete?"true":"false",r.bytes,r.files,r.directories,r.error);return r.complete?0:1;}
    if(argc>1&&wcscmp(argv[1],L"--resolve")==0)return resolveSymbols()?0:1;
    DWORD currentSession=0;ProcessIdToSessionId(GetCurrentProcessId(),&currentSession);
    wchar_t test[2];bool isolated=GetEnvironmentVariableW(L"FOLDERSIZE_TEST",test,2)>0;
    auto eventName=std::wstring(L"Local\\WindowsFolderSize.Stop.")+(isolated?L"Test.":L"")+std::to_wstring(currentSession);
    if(argc>1&&wcscmp(argv[1],L"--stop")==0){HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,eventName.c_str());if(e){SetEvent(e);CloseHandle(e);}return 0;}
    HANDLE stopEvent=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());
    DWORD session;ProcessIdToSessionId(GetCurrentProcessId(),&session);auto mutexName=std::wstring(L"Local\\WindowsFolderSize.Agent.")+(isolated?L"Test.":L"")+std::to_wstring(session);HANDLE mutex=CreateMutexW(nullptr,FALSE,mutexName.c_str());if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS)return 0;
    if(!resolveSymbols()){log(L"Required symbols missing. No Explorer changes made.");CloseHandle(mutex);return 1;}
    log(L"WindowsFolderSize started");std::thread worker(scannerWorker),server(pipeWorker);
    while(!stopping){std::vector<std::wstring> animated;{std::lock_guard lock(cacheMutex);auto now=GetTickCount64();for(auto it=cache.begin();it!=cache.end();){auto& e=it->second;if(!e.running&&!e.queued&&now-e.used>300000){it=cache.erase(it);continue;}if(e.watch&&e.watch->changed()){e.changed=true;e.lastChange=now;}
        // Collapse bursts of writes into a single refresh. For continuously
        // changing folders, refresh at most once every ten seconds.
        if(e.changed&&!e.running&&!e.queued&&(now-e.lastChange>=1000||now-e.finished>=10000))queue(it->first,e);
        if(!e.ready&&(e.queued||e.running)&&now-e.lastAnimation>=600){e.lastAnimation=now;animated.push_back(e.path);}
        ++it;}}for(auto& path:animated)notify(path);if(WaitForSingleObject(stopEvent,200)==WAIT_OBJECT_0)stopping=true;}
    // Exit terminates the blocked pipe listener after the scanner has stopped.
    cv.notify_all();worker.join();server.detach();log(L"WindowsFolderSize stopped");CloseHandle(stopEvent);CloseHandle(mutex);ExitProcess(0);
}
