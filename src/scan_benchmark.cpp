#include "scanner.hpp"
#include "cpu_budget.hpp"
int wmain(int argc,wchar_t**argv){if(argc!=3)return 2;bool old=wcscmp(argv[1],L"old")==0;if(!fsizes::limitCpu(old?100:200))return 3;std::atomic<bool> stop=false;auto start=GetTickCount64();fsizes::ScanJob job(argv[2],stop);for(;;){bool done=job.step(old?64:512,old?2:8);if(old)Sleep(30);if(done)break;}auto r=job.value();printf("%s elapsed_ms=%llu bytes=%llu files=%llu complete=%d\n",old?"1_percent_with_pause":"2_percent_without_pause",GetTickCount64()-start,r.bytes,r.files,r.complete);return r.complete?0:4;}
