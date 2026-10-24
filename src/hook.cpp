// Explorer hook signatures and cache extraction behavior were verified against
// m417z's GPL-3.0 Better file sizes in Explorer details (version 1.6).
// This project is distributed under GPL-3.0-or-later. See THIRD_PARTY.md.
#include "common.hpp"
#include <shlobj.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <propsys.h>
#include <strsafe.h>
#include "MinHook.h"
using namespace fsizes;
static const PROPERTYKEY sizeKey={{0xB725F130,0x47EF,0x101A,{0xA5,0xF1,0x02,0x60,0x8C,0x9E,0xEB,0xAC}},12};
using GetSizeFn=HRESULT(WINAPI*)(void*,const ITEMID_CHILD*,const void*,PROPVARIANT*);
using ItemValueFn=HRESULT(WINAPI*)(void*,const void*,int,const PROPERTYKEY*,PROPVARIANT*,int*);
using ItemPropertyFn=HRESULT(WINAPI*)(void*,void*,const void*,int,const PROPERTYKEY*,int*,PROPVARIANT*);
GetSizeFn originalSize=nullptr;ItemValueFn originalValue=nullptr;ItemPropertyFn originalProperty=nullptr;
decltype(&PSFormatForDisplayAlloc) originalFormatAlloc=nullptr;
decltype(&PSFormatForDisplay) originalFormat=nullptr;
using DescriptionFormatFn=HRESULT(STDMETHODCALLTYPE*)(IPropertyDescription*,REFPROPVARIANT,PROPDESC_FORMAT_FLAGS,PWSTR*);
DescriptionFormatFn originalDescriptionFormat=nullptr;
thread_local bool readingSize=false,insideSize=false;
volatile LONG hookStatus=0;
extern "C" __declspec(dllexport) LONG WindowsFolderSizeHookStatus(){return InterlockedCompareExchange(&hookStatus,0,0);}
bool isSize(const PROPERTYKEY* key){return key&&key->pid==sizeKey.pid&&IsEqualGUID(key->fmtid,sizeKey.fmtid);}
bool pendingDisplay(REFPROPVARIANT value,std::wstring& text){return value.vt==VT_UI8&&pendingSizeText(value.uhVal.QuadPart,text);}
template<class Format> HRESULT approximateFormat(REFPROPVARIANT value,PWSTR* output,Format format){
 if(!output)return E_POINTER;*output=nullptr;PROPVARIANT exact=value;exact.uhVal.QuadPart=plainSize(value.uhVal.QuadPart);PWSTR raw=nullptr;HRESULT hr=format(exact,&raw);if(FAILED(hr))return hr;
 auto text=std::wstring(uiWords().approximate)+(raw?raw:L"");CoTaskMemFree(raw);return SHStrDupW(text.c_str(),output);
}
HRESULT WINAPI formatAlloc(REFPROPERTYKEY key,REFPROPVARIANT value,PROPDESC_FORMAT_FLAGS flags,PWSTR* output){
 std::wstring text;if(isSize(&key)&&pendingDisplay(value,text))return SHStrDupW(text.c_str(),output);if(isSize(&key)){flags=PROPDESC_FORMAT_FLAGS(flags&~PDFF_ALWAYSKB);if(value.vt==VT_UI8&&approximateValue(value.uhVal.QuadPart))return approximateFormat(value,output,[&](REFPROPVARIANT exact,PWSTR* raw){return originalFormatAlloc(key,exact,flags,raw);});}return originalFormatAlloc(key,value,flags,output);
}
HRESULT WINAPI formatValue(REFPROPERTYKEY key,REFPROPVARIANT value,PROPDESC_FORMAT_FLAGS flags,PWSTR output,DWORD count){
 std::wstring text;if(isSize(&key)&&pendingDisplay(value,text))return StringCchCopyW(output,count,text.c_str());if(isSize(&key)){flags=PROPDESC_FORMAT_FLAGS(flags&~PDFF_ALWAYSKB);if(value.vt==VT_UI8&&approximateValue(value.uhVal.QuadPart)){PROPVARIANT exact=value;exact.uhVal.QuadPart=plainSize(value.uhVal.QuadPart);wchar_t raw[256]{};HRESULT hr=originalFormat(key,exact,flags,raw,256);if(FAILED(hr))return hr;text=std::wstring(uiWords().approximate)+raw;return StringCchCopyW(output,count,text.c_str());}}return originalFormat(key,value,flags,output,count);
}
HRESULT STDMETHODCALLTYPE descriptionFormat(IPropertyDescription* description,REFPROPVARIANT value,PROPDESC_FORMAT_FLAGS flags,PWSTR* output){
 PROPERTYKEY key{};if(SUCCEEDED(description->GetPropertyKey(&key))&&isSize(&key)){std::wstring text;if(pendingDisplay(value,text))return SHStrDupW(text.c_str(),output);flags=PROPDESC_FORMAT_FLAGS(flags&~PDFF_ALWAYSKB);if(value.vt==VT_UI8&&approximateValue(value.uhVal.QuadPart))return approximateFormat(value,output,[&](REFPROPVARIANT exact,PWSTR* raw){return originalDescriptionFormat(description,exact,flags,raw);});}return originalDescriptionFormat(description,value,flags,output);
}
bool query(const std::wstring& path,Response& response){
    if(path.size()<3||path.size()>=32000||path[1]!=L':')return false;
    Request req;req.characters=uint32_t(path.size());std::vector<BYTE> data(sizeof(req)+path.size()*sizeof(wchar_t));memcpy(data.data(),&req,sizeof(req));memcpy(data.data()+sizeof(req),path.data(),path.size()*sizeof(wchar_t));
    DWORD n=0;auto name=sessionPipe();
    if(!CallNamedPipeW(name.c_str(),data.data(),DWORD(data.size()),&response,sizeof(response),&n,20))return false;
    return n==sizeof(response)&&response.magicValue==progressMagic;
}
HRESULT WINAPI getSize(void* object,const ITEMID_CHILD* child,const void* folder,PROPVARIANT* value){
    HRESULT hr=originalSize(object,child,folder,value);
    if(insideSize||!child||!value||FAILED(hr)||value->vt!=VT_EMPTY)return hr;
    insideSize=true;
    IShellFolder2* sf=nullptr;
    if(SUCCEEDED(reinterpret_cast<IUnknown*>(object)->QueryInterface(IID_PPV_ARGS(&sf)))){
        STRRET name{};
        if(SUCCEEDED(sf->GetDisplayNameOf(child,SHGDN_FORPARSING,&name))){
            PWSTR path=nullptr;if(SUCCEEDED(StrRetToStrW(&name,child,&path))){Response response;if(query(path,response)){value->vt=VT_UI8;value->uhVal.QuadPart=response.ready?storedSize(response.bytes,response.state==ScanState::approximate):pendingSize(response);}CoTaskMemFree(path);}
        }sf->Release();
    }insideSize=false;return hr;
}
HRESULT WINAPI itemValue(void* object,const void* item,int mode,const PROPERTYKEY* key,PROPVARIANT* value,int* state){
    bool previous=readingSize;readingSize=isSize(key);HRESULT hr=originalValue(object,item,mode,key,value,state);readingSize=previous;return hr;
}
HRESULT WINAPI itemProperty(void* object,void* store,const void* item,int mode,const PROPERTYKEY* key,int* dirty,PROPVARIANT* value){
    HRESULT hr=originalProperty(object,store,item,mode,key,dirty,value);
    if(readingSize&&mode!=0&&(!SUCCEEDED(hr)||(dirty&&*dirty))&&isSize(key)){
        PropVariantClear(value);hr=originalProperty(object,store,item,0,key,nullptr,value);if(SUCCEEDED(hr)&&dirty)*dirty=0;
    }return hr;
}
DWORD WINAPI initialize(void*){
    HMODULE module=nullptr;for(int i=0;i<60&&!module;++i){module=GetModuleHandleW(L"windows.storage.dll");if(!module)Sleep(1000);}if(!module){log(L"Explorer storage module unavailable");return 1;}
    auto base=reinterpret_cast<BYTE*>(module);auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto pe=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);auto ini=dataDirectory()+L"\\hooks.ini";
    if(GetPrivateProfileIntW(L"Symbols",L"TimeStamp",0,ini.c_str())!=pe->FileHeader.TimeDateStamp||GetPrivateProfileIntW(L"Symbols",L"ImageSize",0,ini.c_str())!=pe->OptionalHeader.SizeOfImage){log(L"Explorer version changed; mismatched hooks were not loaded");return 2;}
    DWORD size=GetPrivateProfileIntW(L"Symbols",L"GetSize",0,ini.c_str()),property=GetPrivateProfileIntW(L"Symbols",L"GetItemProperty",0,ini.c_str()),value=GetPrivateProfileIntW(L"Symbols",L"ItemGetValue",0,ini.c_str());
    if(!size||!property||!value||size>=pe->OptionalHeader.SizeOfImage||property>=pe->OptionalHeader.SizeOfImage||value>=pe->OptionalHeader.SizeOfImage)return 3;
    if(MH_Initialize()!=MH_OK)return 4;
    if(MH_CreateHook(base+size,reinterpret_cast<void*>(getSize),reinterpret_cast<void**>(&originalSize))!=MH_OK||MH_CreateHook(base+property,reinterpret_cast<void*>(itemProperty),reinterpret_cast<void**>(&originalProperty))!=MH_OK||MH_CreateHook(base+value,reinterpret_cast<void*>(itemValue),reinterpret_cast<void**>(&originalValue))!=MH_OK){MH_Uninitialize();log(L"Explorer hooks unavailable; original behavior preserved");return 5;}
    // Explorer also calls IPropertyDescription directly; exported PSFormat*
    // hooks alone do not cover that display path. COM is initialized only on
    // this private worker, never on the Explorer thread loading the extension.
    HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);IPropertyDescription* description=nullptr;
    HRESULT described=PSGetPropertyDescription(sizeKey,IID_PPV_ARGS(&description));
    void* formatTarget=description?(*reinterpret_cast<void***>(description))[22]:nullptr;
    bool formatting=SUCCEEDED(described)&&formatTarget&&
        MH_CreateHook(formatTarget,reinterpret_cast<void*>(descriptionFormat),reinterpret_cast<void**>(&originalDescriptionFormat))==MH_OK&&
        MH_CreateHook(reinterpret_cast<void*>(PSFormatForDisplayAlloc),reinterpret_cast<void*>(formatAlloc),reinterpret_cast<void**>(&originalFormatAlloc))==MH_OK&&
        MH_CreateHook(reinterpret_cast<void*>(PSFormatForDisplay),reinterpret_cast<void*>(formatValue),reinterpret_cast<void**>(&originalFormat))==MH_OK;
    if(description)description->Release();if(SUCCEEDED(com))CoUninitialize();
    if(!formatting){MH_Uninitialize();log(L"Progress formatter hooks unavailable; no size hooks enabled");InterlockedExchange(&hookStatus,-1);return 7;}
    HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(initialize),&self);
    if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){log(L"Explorer hook activation failed");return 6;}
    InterlockedExchange(&hookStatus,1);log(L"WindowsFolderSize Explorer hooks active");return 0;
}
INIT_ONCE started=INIT_ONCE_STATIC_INIT;
BOOL CALLBACK startHook(PINIT_ONCE,void*,void**){HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(startHook),&self);HANDLE t=CreateThread(nullptr,0,initialize,nullptr,0,nullptr);if(t)CloseHandle(t);return TRUE;}
extern "C" __declspec(dllexport) void WindowsFolderSizeInitialize(){InitOnceExecuteOnce(&started,startHook,nullptr,nullptr);}
// The shell loads this COM extension itself. The broker never opens or writes
// another process and never creates remote threads.
const CLSID extensionClsid={0x1a249399,0xe61d,0x4d3f,{0xac,0x60,0xbe,0xbb,0xbd,0xbb,0xd1,0x15}};
LONG references=0;
class Overlay final:public IShellIconOverlayIdentifier,public IShellExtInit,public IContextMenu{
    LONG count=1;
public:
    Overlay(){InterlockedIncrement(&references);}
    ~Overlay(){InterlockedDecrement(&references);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IShellIconOverlayIdentifier)*p=static_cast<IShellIconOverlayIdentifier*>(this);else if(iid==IID_IShellExtInit)*p=static_cast<IShellExtInit*>(this);else if(iid==IID_IContextMenu)*p=static_cast<IContextMenu*>(this);if(*p){AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&count);}
    ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&count);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsMemberOf(LPCWSTR,DWORD)override{return S_FALSE;}
    HRESULT STDMETHODCALLTYPE GetOverlayInfo(LPWSTR path,int chars,int*index,DWORD*flags)override{if(!path||!index||!flags)return E_POINTER;wchar_t system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);auto icon=std::wstring(system)+L"\\shell32.dll";if(chars<=int(icon.size()))return E_INVALIDARG;wcscpy_s(path,chars,icon.c_str());*index=0;*flags=ISIOI_ICONFILE|ISIOI_ICONINDEX;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetPriority(int*priority)override{if(!priority)return E_POINTER;*priority=100;return S_OK;}
    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE,IDataObject*,HKEY)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU,UINT,UINT,UINT,UINT)override{return MAKE_HRESULT(SEVERITY_SUCCESS,FACILITY_NULL,0);}
    HRESULT STDMETHODCALLTYPE InvokeCommand(CMINVOKECOMMANDINFO*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR,UINT,UINT*,LPSTR,UINT)override{return E_NOTIMPL;}
};
class Factory final:public IClassFactory{
    LONG count=1;
public:
    Factory(){InterlockedIncrement(&references);}
    ~Factory(){InterlockedDecrement(&references);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**p)override{if(!p)return E_POINTER;*p=nullptr;if(iid==IID_IUnknown||iid==IID_IClassFactory){*p=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&count);}
    ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&count);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown*outer,REFIID iid,void**p)override{if(outer)return CLASS_E_NOAGGREGATION;auto o=new Overlay;HRESULT hr=o->QueryInterface(iid,p);o->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock)override{if(lock)InterlockedIncrement(&references);else InterlockedDecrement(&references);return S_OK;}
};
STDAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void**p){
    if(clsid!=extensionClsid)return CLASS_E_CLASSNOTAVAILABLE;
    wchar_t image[32768];GetModuleFileNameW(nullptr,image,32768);auto file=wcsrchr(image,L'\\');if(file&&_wcsicmp(file+1,L"explorer.exe")==0)WindowsFolderSizeInitialize();
    auto factory=new Factory;HRESULT hr=factory->QueryInterface(iid,p);factory->Release();return hr;
}
STDAPI DllCanUnloadNow(){return hookStatus==1||references?S_FALSE:S_OK;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE;}
