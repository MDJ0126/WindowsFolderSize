#include "common.hpp"
#include <shlobj.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <atomic>
#include <thread>
using namespace fsizes;
std::atomic<unsigned> phase=0;
std::atomic<bool> done=false;
void server(HANDLE pipe){
    while(!done){
        BOOL ok=ConnectNamedPipe(pipe,nullptr);if(!ok&&GetLastError()!=ERROR_PIPE_CONNECTED)continue;
        BYTE data[65536];DWORD n=0;Response response;
        if(ReadFile(pipe,data,sizeof(data),&n,nullptr)){
            unsigned p=phase.load();response.state=p==101?ScanState::failed:p==100?ScanState::queued:p==102?ScanState::complete:ScanState::scanning;
            response.percent=p;response.ready=p==102;response.bytes=4236;
            WriteFile(pipe,&response,sizeof(response),&n,nullptr);FlushFileBuffers(pipe);
        }DisconnectNamedPipe(pipe);
    }CloseHandle(pipe);
}
int wmain(int argc,wchar_t**argv){
    if(argc!=3||!GetEnvironmentVariableW(L"FOLDERSIZE_TEST",nullptr,0))return 2;
    // A deterministic local test server keeps a real folder pending long
    // enough to exercise shell APIs, without altering the production broker.
    auto pipe=CreateNamedPipeW(sessionPipe().c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,64,65536,50,nullptr);
    if(pipe==INVALID_HANDLE_VALUE)return 3;
    std::thread responder(server,pipe);
    auto test=[&]()->int{
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IShellFolder* desktop=nullptr;SHGetDesktopFolder(&desktop);if(desktop)desktop->Release();LoadLibraryW(L"windows.storage.dll");
        auto hook=LoadLibraryW((directory()+L"\\WindowsFolderSizeHook.dll").c_str());if(!hook)return 4;
        auto init=reinterpret_cast<void(*)()>(GetProcAddress(hook,"WindowsFolderSizeInitialize"));auto status=reinterpret_cast<LONG(*)()>(GetProcAddress(hook,"WindowsFolderSizeHookStatus"));if(!init||!status)return 5;init();
        for(unsigned i=0;i<100&&status()==0;i++)Sleep(100);if(status()!=1)return 6;
        PIDLIST_ABSOLUTE parent=nullptr;IShellFolder2* sf=nullptr;PIDLIST_RELATIVE child=nullptr;ULONG eaten=0;DWORD attrs=0;
        if(FAILED(SHParseDisplayName(argv[1],nullptr,&parent,0,nullptr))||FAILED(SHBindToObject(nullptr,parent,nullptr,IID_PPV_ARGS(&sf)))||FAILED(sf->ParseDisplayName(nullptr,nullptr,argv[2],&eaten,&child,&attrs)))return 7;
        IPropertyDescription* description=nullptr;if(FAILED(PSGetPropertyDescription(PKEY_Size,IID_PPV_ARGS(&description))))return 8;
        for(unsigned p: {100u,0u,37u,99u,101u,102u}){
            phase=p;VARIANT value{};HRESULT hr=sf->GetDetailsEx(child,&PKEY_Size,&value);
            if(FAILED(hr)||value.vt!=VT_UI8){printf("Shell value failed phase=%u hr=%08lx type=%u\n",p,hr,value.vt);return 9;}
            PROPVARIANT canonical{};InitPropVariantFromUInt64(value.ullVal,&canonical);VariantClear(&value);
            if(FAILED(description->CoerceToCanonicalValue(&canonical)))return 10;
            PWSTR text=nullptr;hr=description->FormatForDisplay(canonical,PDFF_DEFAULT,&text);
            std::wstring expected;if(p==102){if(canonical.uhVal.QuadPart!=4236)return 11;}else if(!pendingSizeText(canonical.uhVal.QuadPart,expected))return 12;
            if(FAILED(hr)||!text||!*text||(p!=102&&expected!=text)){printf("Shell display failed phase=%u hr=%08lx\n",p,hr);return 13;}
            printf("PASS real folder -> shell property -> canonical conversion -> COM display: phase %u\n",p);CoTaskMemFree(text);PropVariantClear(&canonical);
        }
        phase=37;bool columnFound=false;
        for(UINT column=0;column<100;column++){
            PROPERTYKEY key{};if(FAILED(sf->MapColumnToSCID(column,&key)))break;
            if(key.pid!=PKEY_Size.pid||!IsEqualGUID(key.fmtid,PKEY_Size.fmtid))continue;
            columnFound=true;SHELLDETAILS details{};HRESULT hr=sf->GetDetailsOf(child,column,&details);PWSTR text=nullptr;
            if(SUCCEEDED(hr))hr=StrRetToStrW(&details.str,child,&text);
            bool ok=SUCCEEDED(hr)&&text&&(*text==L'.')&&wcsstr(text,L"37%")&&wcslen(text)<=6;
            CoTaskMemFree(text);if(!ok){printf("Native size column failed hr=%08lx\n",hr);return 15;}
            puts("PASS native shell Size column returns progress text");break;
        }
        if(!columnFound)return 16;
        // Property-store retrieval is another shell path that canonicalizes
        // values. Use a fresh store to avoid intentionally cached snapshots.
        phase=37;auto path=std::wstring(argv[1])+L"\\"+argv[2];IShellItem2* item=nullptr;IPropertyStore* store=nullptr;
        HRESULT hr=SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(&item));
        if(SUCCEEDED(hr))hr=item->GetPropertyStore(GPS_DEFAULT,IID_PPV_ARGS(&store));
        PROPVARIANT stored{};if(SUCCEEDED(hr))hr=store->GetValue(PKEY_Size,&stored);
        std::wstring storedText;
        if(FAILED(hr)||stored.vt!=VT_UI8||!pendingSizeText(stored.uhVal.QuadPart,storedText)||storedText.find(L"37%")==std::wstring::npos){printf("Property store failed hr=%08lx type=%u\n",hr,stored.vt);return 14;}
        puts("PASS real folder IPropertyStore retains pending progress");PropVariantClear(&stored);store->Release();item->Release();description->Release();CoTaskMemFree(child);CoTaskMemFree(parent);sf->Release();CoUninitialize();return 0;
    };
    int result=test();done=true;DWORD n=0;Response response;Request wake;
    CallNamedPipeW(sessionPipe().c_str(),&wake,sizeof(wake),&response,sizeof(response),&n,2000);responder.join();return result;
}
