#include "common.hpp"
#include <shlobj.h>
#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>
#include <iostream>
using namespace fsizes;
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IPropertyDescription* description=nullptr;if(FAILED(PSGetPropertyDescription(PKEY_Size,IID_PPV_ARGS(&description))))return 15;
    PROPVARIANT oldValue{};InitPropVariantFromString(L"계산 중 · 37%",&oldValue);
    HRESULT oldCanonical=description->CoerceToCanonicalValue(&oldValue);
    if(SUCCEEDED(oldCanonical)||oldValue.vt!=VT_EMPTY){puts("Old string regression not reproduced");return 16;}
    puts("REPRODUCED old progress string discarded by System.Size canonicalization");PropVariantClear(&oldValue);
    IShellFolder* desktop=nullptr;SHGetDesktopFolder(&desktop);if(desktop)desktop->Release();
    LoadLibraryW(L"windows.storage.dll");
    HMODULE hook=LoadLibraryW((directory()+L"\\WindowsFolderSizeHook.dll").c_str());if(!hook){printf("Load failed %lu\n",GetLastError());return 3;}
    auto initialize=reinterpret_cast<void(*)()>(GetProcAddress(hook,"WindowsFolderSizeInitialize"));if(!initialize)return 3;initialize();
    auto status=reinterpret_cast<LONG(*)()>(GetProcAddress(hook,"WindowsFolderSizeHookStatus"));
    for(int i=0;i<100&&status&&status()!=1;i++)Sleep(100);
    if(!status||status()!=1){puts("Hook not ready");return 4;}
    PIDLIST_ABSOLUTE parent=nullptr;IShellFolder2* sf=nullptr;
    if(FAILED(SHParseDisplayName(argv[1],nullptr,&parent,0,nullptr))||FAILED(SHBindToObject(nullptr,parent,nullptr,IID_PPV_ARGS(&sf))))return 5;
    PIDLIST_RELATIVE child=nullptr;ULONG eaten=0;DWORD attrs=0;
    if(FAILED(sf->ParseDisplayName(nullptr,nullptr,argv[2],&eaten,&child,&attrs)))return 6;
    VARIANT initial{};HRESULT initialHr=sf->GetDetailsEx(child,&PKEY_Size,&initial);
    std::wstring initialText;
    if(FAILED(initialHr)||initial.vt!=VT_UI8||!pendingSizeText(initial.ullVal,initialText)){printf("Pending numeric state missing hr=%08lx type=%u\n",initialHr,initial.vt);VariantClear(&initial);return 12;}
    puts("PASS pending numeric state returned through Windows folder property API");VariantClear(&initial);
    // Exercise Windows' canonicalization and both COM/exported display paths,
    // including every percentage, queued/error states, and an ordinary size.
    for(unsigned code=0;code<=101;code++){
        Response progress;progress.state=code==101?ScanState::failed:code==100?ScanState::queued:ScanState::scanning;progress.percent=code;
        auto expected=progressText(progress);PROPVARIANT statusValue{};InitPropVariantFromUInt64(pendingSize(progress,2),&statusValue);
        auto before=statusValue.uhVal.QuadPart;
        if(FAILED(description->CoerceToCanonicalValue(&statusValue))||statusValue.vt!=VT_UI8||statusValue.uhVal.QuadPart!=before||description->IsValueCanonical(statusValue)!=S_OK)return 17;
        PWSTR display=nullptr;HRESULT hr=description->FormatForDisplay(statusValue,PDFF_ALWAYSKB,&display);
        bool ok=SUCCEEDED(hr)&&display&&expected==display;CoTaskMemFree(display);if(!ok){printf("COM format failed code=%u hr=%08lx\n",code,hr);return 18;}
        display=nullptr;hr=PSFormatForDisplayAlloc(PKEY_Size,statusValue,PDFF_ALWAYSKB,&display);
        ok=SUCCEEDED(hr)&&display&&expected==display;CoTaskMemFree(display);if(!ok)return 19;
        wchar_t buffer[80];if(FAILED(PSFormatForDisplay(PKEY_Size,statusValue,PDFF_DEFAULT,buffer,80))||expected!=buffer)return 20;
        PropVariantClear(&statusValue);
    }
    PROPVARIANT ordinary{};InitPropVariantFromUInt64(4236,&ordinary);PWSTR ordinaryText=nullptr;
    HRESULT ordinaryHr=description->FormatForDisplay(ordinary,PDFF_ALWAYSKB,&ordinaryText);
    bool normal=SUCCEEDED(ordinaryHr)&&ordinaryText&&*ordinaryText&&!wcsstr(ordinaryText,L"%");CoTaskMemFree(ordinaryText);PropVariantClear(&ordinary);if(!normal)return 21;
    description->Release();puts("PASS canonicalization + COM/exported formatters: 0..99 percent, queued, failed, ordinary bytes");
    auto read=[&](uint64_t expected){
        for(int i=0;i<100;i++){VARIANT v{};HRESULT hr=sf->GetDetailsEx(child,&PKEY_Size,&v);if(SUCCEEDED(hr)&&v.vt==VT_UI8&&v.ullVal==expected){VariantClear(&v);printf("PASS bytes=%llu\n",expected);return true;}VariantClear(&v);Sleep(100);}return false;
    };
    if(!read(4236)){puts("Initial size mismatch");return 7;}
    std::wstring changed=std::wstring(argv[1])+L"\\"+argv[2]+L"\\change-test.bin";
    HANDLE f=CreateFileW(changed.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return 8;DWORD n;char data[31]{};WriteFile(f,data,31,&n,nullptr);CloseHandle(f);
    if(!read(4267)){puts("Change detection mismatch");return 9;}
    if(!DeleteFileW(changed.c_str()))return 10;
    if(!read(4236)){puts("Delete detection mismatch");return 11;}
    std::wstring fixture=std::wstring(argv[1])+L"\\"+argv[2];Request legacyRequest;legacyRequest.magicValue=magic;legacyRequest.characters=uint32_t(fixture.size());std::vector<BYTE> packet(sizeof(legacyRequest)+fixture.size()*sizeof(wchar_t));memcpy(packet.data(),&legacyRequest,sizeof(legacyRequest));memcpy(packet.data()+sizeof(legacyRequest),fixture.data(),fixture.size()*sizeof(wchar_t));LegacyResponse legacy{};DWORD received=0;
    if(!CallNamedPipeW(sessionPipe().c_str(),packet.data(),DWORD(packet.size()),&legacy,sizeof(legacy),&received,2000)||received!=sizeof(legacy)||legacy.magicValue!=magic||!legacy.ready||legacy.bytes!=4236){puts("Legacy protocol compatibility failed");return 14;}puts("PASS existing extension protocol remains compatible");
    CoTaskMemFree(child);CoTaskMemFree(parent);sf->Release();CoUninitialize();puts("Integration tests passed");return 0;
}
