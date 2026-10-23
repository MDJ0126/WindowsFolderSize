#include "scanner.hpp"
#include <cassert>
using namespace fsizes;
int wmain(int argc,wchar_t**argv){if(argc!=2)return 2;std::atomic<bool> stop=false;std::vector<unsigned> values;
auto r=scan(argv[1],stop,[&](unsigned p,uint64_t){if(!values.empty()&&p<values.back()){puts("Progress decreased");ExitProcess(3);}values.push_back(p);});
bool intermediate=false;for(auto p:values){printf("%u ",p);if(p>0&&p<100)intermediate=true;}puts("");
if(!r.complete||r.bytes!=4236||values.empty()||values.front()!=0||values.back()!=100||!intermediate)return 4;
values.clear();ScanJob sliced(argv[1],stop,[&](unsigned p,uint64_t){if(!values.empty()&&p<values.back())ExitProcess(9);values.push_back(p);});
ScanJob smallJob(std::wstring(argv[1])+L"\\empty",stop);bool smallFinished=false,largeFinished=false;unsigned rounds=0;
while(!largeFinished){largeFinished=sliced.step(1,1000);if(!smallFinished){smallFinished=smallJob.step(1,1000);if(smallFinished&&largeFinished)return 10;}if(++rounds>10000)return 11;}
if(!smallFinished||!sliced.value().complete||sliced.value().bytes!=r.bytes||sliced.value().files!=r.files||sliced.value().directories!=r.directories)return 12;
puts("PASS resumable enumeration: small folder finishes before larger folder; exact bytes/files/directories preserved");
std::atomic<bool> cancel=false;ScanJob cancelled(argv[1],cancel);cancelled.step(1,1000);cancel=true;if(!cancelled.step()||cancelled.value().complete||cancelled.value().error!=ERROR_CANCELLED)return 13;
values.clear();auto bad=scan(std::wstring(argv[1])+L"\\missing",stop,[&](unsigned p,uint64_t){values.push_back(p);});if(bad.complete||values.back()==100)return 5;
Response state;state.state=ScanState::queued;if(progressText(state)!=L"...0%")return 6;state.state=ScanState::failed;if(progressText(state)!=L"계산 불가")return 7;
state.state=ScanState::scanning;state.percent=37;
for(unsigned frame=0;frame<3;frame++){std::wstring text;if(!pendingSizeText(pendingSize(state,frame),text)||text!=std::wstring(frame+1,L'.')+L"37%")return 8;}
puts("PASS animation frames .37%, ..37%, ...37%");
puts("PASS monotonic measured traversal progress, completed bytes, error state");return 0;}
