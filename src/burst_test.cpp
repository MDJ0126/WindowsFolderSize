#include "common.hpp"
#include "file_index.hpp"
#include <filesystem>
using namespace fsizes;
bool query(const std::wstring& path,Response& r){Request q;q.characters=uint32_t(path.size());std::vector<BYTE>d(sizeof(q)+path.size()*2);memcpy(d.data(),&q,sizeof(q));memcpy(d.data()+sizeof(q),path.data(),path.size()*2);DWORD n=0;return CallNamedPipeW(sessionPipe().c_str(),d.data(),DWORD(d.size()),&r,sizeof(r),&n,1000)&&n==sizeof(r);}
HANDLE launch(const wchar_t* args){auto exe=directory()+L"\\bin\\WindowsFolderSize.exe";auto command=L"\""+exe+L"\" "+args;STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return nullptr;CloseHandle(pi.hThread);return pi.hProcess;}
void stop(HANDLE h){HANDLE p=launch(L"--stop");if(p){WaitForSingleObject(p,6000);CloseHandle(p);}WaitForSingleObject(h,6000);CloseHandle(h);}
int wmain(){SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");auto root=directory()+L"\\burst-fixture-"+std::to_wstring(GetCurrentProcessId());std::filesystem::create_directories(root+L"\\many");
 for(unsigned i=0;i<2000;i++){auto p=root+L"\\many\\"+std::to_wstring(i);HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);DWORD n;char b=0;WriteFile(h,&b,1,&n,nullptr);CloseHandle(h);}
 HANDLE broker=launch(L"");if(!broker)return 2;Response r{};bool ready=false;for(int i=0;i<300;i++){if(query(root,r)&&r.ready&&r.bytes==2000){ready=true;break;}Sleep(20);}if(!ready){stop(broker);return 3;}
 auto started=GetTickCount64();std::filesystem::rename(root+L"\\many",root+L"\\renamed");Sleep(400);std::filesystem::remove_all(root+L"\\renamed");bool final=false,recovery=false;
 for(int i=0;i<500;i++){if(query(root,r)){if(!r.ready)recovery=true;if(r.ready&&r.bytes==0){final=true;break;}}Sleep(20);}auto elapsed=GetTickCount64()-started;
 FileSizes files;uint64_t total=99;bool approx=false;bool durable=readIndex(fileKey(root),files,total,approx)&&total==0&&files.empty();stop(broker);std::filesystem::remove(root);
 printf("2000 real files: rename + subtree deletion, final=%d durable=%d recovery=%d elapsed=%llu ms\n",final,durable,recovery,elapsed);if(!final||!durable)return 4;puts("PASS burst changes converge to exact durable zero");return 0;
}
