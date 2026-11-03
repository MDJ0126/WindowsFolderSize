#include "disk_index.hpp"
#include "cpu_budget.hpp"
#include <psapi.h>
using namespace fsizes;
uint64_t cpu(){FILETIME c,e,k,u;GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u);ULARGE_INTEGER a,b;a.LowPart=k.dwLowDateTime;a.HighPart=k.dwHighDateTime;b.LowPart=u.dwLowDateTime;b.HighPart=u.dwHighDateTime;return a.QuadPart+b.QuadPart;}
int wmain(int argc,wchar_t** argv){
 if(!limitCpu(100))return 1;SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");auto start=GetTickCount64(),ct=cpu();auto root=L"C:\\resource-test\\"+std::wstring(100,L'x');unsigned count=argc>1&&wcscmp(argv[1],L"quick")==0?10000:150000;uint64_t total=uint64_t(count)*(count+1)/2;
 if(argc>1&&wcscmp(argv[1],L"legacy")==0){FileSizes map;for(unsigned i=0;i<count;i++)map[root+L"\\tree\\f"+std::to_wstring(i)]=i+1;FileSizes duplicate=map;printf("legacy resident records=%zu (plus recovery copy)\n",map.size());}
 else{
  auto db=directory()+L"\\resource-test-"+std::to_wstring(GetCurrentProcessId())+L".sqlite";
  {DiskIndex index(db);if(!index.good()||!index.startScan(root))return 2;
   for(unsigned i=0;i<count;){if(!index.begin())return 3;for(unsigned end=(std::min)(count,i+256);i<end;i++)index.observe(root,root+L"\\tree\\f"+std::to_wstring(i),i+1);if(!index.commit())return 4;Sleep(8);}
   if(!index.finishScan(root))return 5;bool ok;auto n=index.total(root,ok);if(!ok||n!=total)return 6;
   if(index.cacheBytes()>3*1024*1024)return 7;printf("disk cache=%lld bytes, exact records=%u total=%llu\n",index.cacheBytes(),count,n);
   bool found=false;if(!index.begin()||!index.move(root,root+L"\\tree",root+L"\\renamed",total,found)||!found||!index.commit())return 8;
   if(!index.begin()||!index.put(root,root+L"\\extra",17,total)||!index.commit())return 9;
   if(!index.begin()||!index.remove(root,root+L"\\renamed",total)||!index.commit()||total!=17)return 10;
   if(!index.begin()||!index.put(root,root+L"\\rollback",30,total))return 11;index.rollback();if(index.total(root,ok)!=17||!ok)return 12;
   auto actual=directory()+L"\\delta-fixture-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(actual.c_str(),nullptr);auto file=actual+L"\\payload.bin";uint64_t amount=0;std::atomic<bool> stop=false;bool approx=false;
   if(!index.startScan(actual)||!index.finishScan(actual))return 14;
   for(unsigned size:{7u,19u}){HANDLE f=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(f==INVALID_HANDLE_VALUE)return 15;SetFilePointer(f,size,nullptr,FILE_BEGIN);SetEndOfFile(f);CloseHandle(f);if(!index.begin()||!index.delta(actual,file,FILE_ACTION_MODIFIED,amount,stop,approx)||!index.commit()||amount!=size)return 16;}
   DeleteFileW(file.c_str());if(!index.begin()||!index.delta(actual,file,FILE_ACTION_REMOVED,amount,stop,approx)||!index.commit()||amount!=0)return 17;RemoveDirectoryW(actual.c_str());puts("PASS actual file add/resize/delete arithmetic");

  }
  {DiskIndex reopen(db);bool ok;if(reopen.total(root,ok)!=17||!ok)return 13;}
  DeleteFileW(db.c_str());DeleteFileW((db+L"-wal").c_str());DeleteFileW((db+L"-shm").c_str());puts("PASS persistent reopen, rename, add, subtree delete, rollback");
 }
 PROCESS_MEMORY_COUNTERS_EX m{};GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m));auto wall=GetTickCount64()-start;SYSTEM_INFO info;GetSystemInfo(&info);double pct=wall?100.0*double(cpu()-ct)/(10000.0*wall*info.dwNumberOfProcessors):0;printf("peak_working_set=%zu private_now=%zu cpu_system_pct=%.3f elapsed_ms=%llu\n",m.PeakWorkingSetSize,m.PrivateUsage,pct,wall);return 0;
}

