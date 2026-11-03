#define wmain unused_broker_main
#include "agent.cpp"
#undef wmain
#include <cstdio>
bool waitSize(const std::wstring& root,uint64_t bytes){for(int i=0;i<100;i++){{std::lock_guard lock(cacheMutex);auto& e=cache[root];if(e.ready&&e.bytes==bytes&&!e.running&&!e.queued)return true;}Sleep(50);}return false;}
int wmain(){SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");if(!limitCpu(100))return 1;auto root=fileKey(directory()+L"\\broker-resource-fixture-"+std::to_wstring(GetCurrentProcessId()));CreateDirectoryW(root.c_str(),nullptr);auto path=root+L"\\payload.bin";auto write=[&](DWORD bytes){HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;SetFilePointer(h,bytes,nullptr,FILE_BEGIN);bool ok=SetEndOfFile(h);CloseHandle(h);return ok;};if(!write(7))return 2;
 {std::lock_guard lock(cacheMutex);Entry e;e.path=root;auto it=cache.emplace(root,std::move(e)).first;queue(root,it->second);}std::thread worker(scannerWorker);
 bool initial=waitSize(root,7);bool resize=false,removed=false;
 if(initial&&write(19)){{std::lock_guard lock(cacheMutex);auto& e=cache[root];e.changes[L"payload.bin"]=FILE_ACTION_MODIFIED;queue(root,e);if(e.recovering)ExitProcess(6);}resize=waitSize(root,19);}
 if(resize&&DeleteFileW(path.c_str())){{std::lock_guard lock(cacheMutex);auto& e=cache[root];e.changes[L"payload.bin"]=FILE_ACTION_REMOVED;queue(root,e);}removed=waitSize(root,0);}
 stopping=true;cv.notify_all();worker.join();DeleteFileW(path.c_str());RemoveDirectoryW(root.c_str());printf("production scanner initial=%d resized=%d deleted=%d no_recovery_on_delta=1\n",initial,resize,removed);return initial&&resize&&removed?0:7;
}
