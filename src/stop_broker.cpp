#include "common.hpp"
#include <tlhelp32.h>
#include <restartmanager.h>
#include <fstream>
using namespace fsizes;
bool brokerImage(const wchar_t* path,const std::wstring& target){
 if(!_wcsicmp(path,target.c_str()))return true;
 auto rollback=target.substr(0,2)+L"\\Config.Msi\\";size_t length=wcslen(path);if(_wcsnicmp(path,rollback.c_str(),rollback.size())||length<4||_wcsicmp(path+length-4,L".rbf"))return false;
 DWORD ignored=0,n=GetFileVersionInfoSizeW(path,&ignored);if(!n)return false;std::vector<BYTE> data(n);if(!GetFileVersionInfoW(path,0,n,data.data()))return false;
 wchar_t* name=nullptr;UINT chars=0;return VerQueryValueW(data.data(),L"\\StringFileInfo\\040904B0\\ProductName",reinterpret_cast<void**>(&name),&chars)&&chars&&!wcscmp(name,L"WindowsFolderSize");
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
 PWSTR programFiles=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&programFiles)))return 2;
 auto target=std::wstring(programFiles)+L"\\WindowsFolderSize\\WindowsFolderSize.exe";CoTaskMemFree(programFiles);
 wchar_t test[2];bool isolated=GetEnvironmentVariableW(L"FOLDERSIZE_TEST",test,2)>0;if(isolated)target=directory()+L"\\WindowsFolderSize.exe";
 std::wofstream diagnostic(dataDirectory()+L"\\BrokerStop.log");diagnostic<<L"Target: "<<target<<L"\n"; HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return 3;
 std::vector<RM_UNIQUE_PROCESS> apps;std::vector<HANDLE> handles;PROCESSENTRY32W item{sizeof(item)};
 if(Process32FirstW(snapshot,&item))do{if(_wcsicmp(item.szExeFile,L"WindowsFolderSize.exe"))continue;HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,item.th32ProcessID);if(!h){diagnostic<<item.th32ProcessID<<L" open failed "<<GetLastError()<<L"\n";continue;}wchar_t path[32768];DWORD length=32768;FILETIME created,exit,kernel,user;
  bool queried=QueryFullProcessImageNameW(h,0,path,&length)!=FALSE;diagnostic<<item.th32ProcessID<<L" path="<<(queried?path:L"unavailable")<<L"\n";if(queried&&(isolated?!_wcsicmp(path,target.c_str()):brokerImage(path,target))&&GetProcessTimes(h,&created,&exit,&kernel,&user)){RM_UNIQUE_PROCESS app{};app.dwProcessId=item.th32ProcessID;app.ProcessStartTime=created;apps.push_back(app);handles.push_back(h);}else CloseHandle(h);
 }while(Process32NextW(snapshot,&item));CloseHandle(snapshot);if(apps.empty())return 0;
 DWORD session=0;wchar_t key[CCH_RM_SESSION_KEY+1]{};DWORD error=RmStartSession(&session,0,key);
 if(!error){error=RmRegisterResources(session,0,nullptr,DWORD(apps.size()),apps.data(),0,nullptr);if(!error)error=RmShutdown(session,0,nullptr);RmEndSession(session);}
 for(auto h:handles){if(WaitForSingleObject(h,3000)!=WAIT_OBJECT_0&&!error)error=ERROR_TIMEOUT;CloseHandle(h);}return error?1:0;
}
