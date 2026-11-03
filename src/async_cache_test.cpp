#include "async_size_cache.hpp"
#include <algorithm>
using namespace fsizes;
double micros(LARGE_INTEGER start){LARGE_INTEGER end,frequency;QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);return double(end.QuadPart-start.QuadPart)*1000000.0/frequency.QuadPart;}
bool pipeTest(bool bounded){
 HANDLE pipe=CreateNamedPipeW(sessionPipe().c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,1,128,65536,50,nullptr);if(pipe==INVALID_HANDLE_VALUE)return false;
 std::thread server([&]{BOOL connected=ConnectNamedPipe(pipe,nullptr);if(connected||GetLastError()==ERROR_PIPE_CONNECTED){BYTE packet[65536];DWORD n;ReadFile(pipe,packet,sizeof(packet),&n,nullptr);Sleep(350);Response response;response.ready=1;response.bytes=42;WriteFile(pipe,&response,sizeof(response),&n,nullptr);}DisconnectNamedPipe(pipe);CloseHandle(pipe);});
 std::wstring path=L"C:\\fixture";Response response{};LARGE_INTEGER start;QueryPerformanceCounter(&start);bool ok;
 if(bounded)ok=boundedSizeQuery(path,response,100);else{Request request;request.characters=uint32_t(path.size());std::vector<BYTE> data(sizeof(request)+path.size()*2);memcpy(data.data(),&request,sizeof(request));memcpy(data.data()+sizeof(request),path.data(),path.size()*2);DWORD n;ok=CallNamedPipeW(sessionPipe().c_str(),data.data(),DWORD(data.size()),&response,sizeof(response),&n,20)!=FALSE;}
 double elapsed=micros(start)/1000;server.join();printf("%s: %.2f ms, result=%d\n",bounded?"bounded worker transaction (100ms deadline)":"baseline CallNamedPipe(timeout=20ms), delayed response",elapsed,ok);return bounded?(!ok&&elapsed<250):(ok&&elapsed>=300);
}
int wmain(){SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");if(!pipeTest(false))return 2;if(!pipeTest(true))return 3;
 HANDLE release=CreateEventW(nullptr,TRUE,FALSE,nullptr),entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);double worst=0;DWORD caller=GetCurrentThreadId();std::atomic<bool> wrongThread=false;
 {AsyncSizeCache cache([&](const std::wstring&,Response& response){if(GetCurrentThreadId()==caller)wrongThread=true;SetEvent(entered);WaitForSingleObject(release,5000);response.ready=1;response.bytes=42;return true;});Response response;cache.read(L"C:\\first",response);WaitForSingleObject(entered,1000);
  LARGE_INTEGER total;QueryPerformanceCounter(&total);for(unsigned i=0;i<10000;i++){LARGE_INTEGER at;auto path=L"C:\\folders\\"+std::to_wstring(i);QueryPerformanceCounter(&at);cache.read(path,response);worst=(std::max)(worst,micros(at));}double elapsed=micros(total)/1000;SetEvent(release);printf("10000 UI reads while worker blocked: total=%.2f ms, worst=%.2f us, caller_did_IO=%d\n",elapsed,worst,wrongThread.load());if(elapsed>2000||worst>20000||wrongThread)return 4;}
 CloseHandle(entered);CloseHandle(release);
 std::atomic<unsigned> value=123,notifications=0;std::atomic<bool> available=true;bool pass=true;
 {AsyncSizeCache cache([&](const std::wstring&,Response& r){if(!available)return false;r.ready=1;r.bytes=value.load();r.state=ScanState::complete;return true;},[&](const std::wstring&){notifications++;});Response r;
  auto expect=[&](unsigned bytes){for(unsigned i=0;i<200;i++){cache.read(L"C:\\cached",r);if(r.ready&&r.bytes==bytes)return true;Sleep(5);}return false;};pass=expect(123);value=456;pass=pass&&expect(456);available=false;Sleep(220);for(unsigned i=0;i<100;i++){cache.read(L"C:\\cached",r);if(!r.ready||r.bytes!=456)pass=false;Sleep(3);}printf("cache update + unavailable broker retains numeric 456: %s, notifications=%u\n",pass?"PASS":"FAIL",notifications.load());}
 if(!pass)return 5;puts("PASS nonblocking multi-folder cache, bounded IPC, worker-only I/O and numeric retention");return 0;
}
