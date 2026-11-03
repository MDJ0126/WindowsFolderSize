#pragma once
#include "common.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>

namespace fsizes {
// This function runs ONLY on the dedicated worker, never on an Explorer
// property/paint thread. The deadline includes the response transaction.
inline bool boundedSizeQuery(const std::wstring& path,Response& response,DWORD timeout=100){
 Request request;request.characters=uint32_t(path.size());
 std::vector<BYTE> data(sizeof(request)+path.size()*sizeof(wchar_t));
 memcpy(data.data(),&request,sizeof(request));memcpy(data.data()+sizeof(request),path.data(),path.size()*sizeof(wchar_t));
 HANDLE pipe=CreateFileW(sessionPipe().c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
 if(pipe==INVALID_HANDLE_VALUE)return false;
 DWORD mode=PIPE_READMODE_MESSAGE;
 if(!SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr)){CloseHandle(pipe);return false;}
 OVERLAPPED operation{};operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 if(!operation.hEvent){CloseHandle(pipe);return false;}
 DWORD bytes=0;bool ok=TransactNamedPipe(pipe,data.data(),DWORD(data.size()),&response,sizeof(response),&bytes,&operation)!=FALSE;
 if(!ok&&GetLastError()==ERROR_IO_PENDING){
  ok=GetOverlappedResultEx(pipe,&operation,&bytes,timeout,FALSE)!=FALSE;
  if(!ok){CancelIoEx(pipe,&operation);GetOverlappedResult(pipe,&operation,&bytes,TRUE);}
 }
 CloseHandle(operation.hEvent);CloseHandle(pipe);
 return ok&&bytes==sizeof(response)&&response.magicValue==progressMagic;
}

class AsyncSizeCache {
 struct Entry {Response response{};ULONGLONG used=0,due=0,checked=0;bool pending=true,inFlight=false;};
 std::map<std::wstring,Entry> entries;
 std::mutex mutex;std::condition_variable changed;
 std::atomic<bool> stopping=false;
 std::function<bool(const std::wstring&,Response&)> resolve;
 std::function<void(const std::wstring&)> notify;
 inline static std::atomic<uint64_t> identifiers{0};
 const uint64_t identifier=++identifiers;
 std::thread worker;
 static bool different(const Response& a,const Response& b){return a.ready!=b.ready||a.bytes!=b.bytes||a.percent!=b.percent||a.state!=b.state;}
 void run(){
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
  HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  while(!stopping){
   std::wstring path;
   {std::unique_lock lock(mutex);auto now=GetTickCount64();auto chosen=entries.end();
    for(auto it=entries.begin();it!=entries.end();++it)if(it->second.pending&&!it->second.inFlight&&it->second.due<=now&&(chosen==entries.end()||it->second.due<chosen->second.due))chosen=it;
    if(chosen==entries.end()){changed.wait_for(lock,std::chrono::milliseconds(20));continue;}
    path=chosen->first;chosen->second.inFlight=true;chosen->second.pending=false;
   }
   Response response{};bool ok=resolve(path,response),updated=false;
   {std::lock_guard lock(mutex);auto it=entries.find(path);if(it!=entries.end()){
    auto& entry=it->second;entry.inFlight=false;entry.checked=GetTickCount64();
    if(ok){updated=different(entry.response,response);entry.response=response;}
    // Preserve the last numeric value on broker failure. Retry a pending
    // initial calculation while the shell still uses this row.
    if((!ok||!response.ready)&&entry.checked-entry.used<5000){entry.pending=true;entry.due=entry.checked+250;}
   }}
   if(updated&&!stopping&&notify)notify(path);
   // A single bounded-rate worker avoids a thread per visible folder and
   // prevents a fast broker from turning shell refreshes into a busy loop.
   {std::unique_lock lock(mutex);changed.wait_for(lock,std::chrono::milliseconds(16),[this]{return stopping.load();});}
  }
  if(SUCCEEDED(com))CoUninitialize();
 }
public:
 AsyncSizeCache(std::function<bool(const std::wstring&,Response&)> resolver,std::function<void(const std::wstring&)> notification={}):resolve(std::move(resolver)),notify(std::move(notification)),worker([this]{run();}){}
 ~AsyncSizeCache(){stopping=true;changed.notify_all();if(worker.joinable())worker.join();}
 AsyncSizeCache(const AsyncSizeCache&)=delete;
 // No IPC, filesystem operations, sleeps or blocking lock acquisitions here.
 // Allocation is bounded by the cache limit, independent of broker speed.
 bool read(const std::wstring& path,Response& output){
  // Keep each caller's last snapshot available even while the worker is
  // publishing. Contention must not turn a completed numeric value into 0%.
  struct Local {uint64_t owner=0;std::map<std::wstring,Response> snapshots;};
  thread_local Local local;
  if(local.owner!=identifier){local.snapshots.clear();local.owner=identifier;}
  auto saved=local.snapshots.find(path);output=saved==local.snapshots.end()?Response{}:saved->second;
  std::unique_lock lock(mutex,std::try_to_lock);if(!lock.owns_lock())return true;
  auto it=entries.find(path);auto now=GetTickCount64();
  if(it==entries.end()){
   if(entries.size()>=1024){auto oldest=entries.end();for(auto candidate=entries.begin();candidate!=entries.end();++candidate)if(!candidate->second.inFlight&&(oldest==entries.end()||candidate->second.used<oldest->second.used))oldest=candidate;if(oldest==entries.end())return true;entries.erase(oldest);}
   it=entries.emplace(path,Entry{}).first;it->second.due=now;
  }
  auto& entry=it->second;entry.used=now;output=entry.response;
  if(saved==local.snapshots.end()&&local.snapshots.size()>=1024)local.snapshots.erase(local.snapshots.begin());
  local.snapshots[path]=output;
  if(!entry.inFlight&&now-entry.checked>=200){entry.pending=true;if(entry.due<now)entry.due=now;changed.notify_one();}
  return true;
 }
};
}
