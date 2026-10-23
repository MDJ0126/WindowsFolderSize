#include "common.hpp"
#include <dbghelp.h>
#include <array>
using namespace fsizes;
struct Symbol { const char* name; const wchar_t* key; DWORD rva=0; };
struct Context { DWORD64 base; std::array<Symbol,3> symbols{{{"CFSFolder::_GetSize",L"GetSize"},{"CDefCollection::_GetItemProperty",L"GetItemProperty"},{"CDefCollection::Item_GetValue",L"ItemGetValue"}}}; };
BOOL CALLBACK found(PSYMBOL_INFO s,ULONG,PVOID v){auto& c=*static_cast<Context*>(v);for(auto& target:c.symbols)if(strcmp(s->Name,target.name)==0)target.rva=DWORD(s->Address-c.base);return TRUE;}
int wmain(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto data=dataDirectory();auto cache=data+L"\\symbols";CreateDirectoryW(cache.c_str(),nullptr);
    auto path=L"srv*"+cache+L"*https://msdl.microsoft.com/download/symbols";
    SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS);
    auto process=GetCurrentProcess();if(!SymInitializeW(process,path.c_str(),FALSE)){log(L"Symbol resolver initialization failed");return 1;}
    auto storage=storagePath();Context c{};
    c.base=SymLoadModuleExW(process,nullptr,storage.c_str(),L"storage",0,0,nullptr,0);
    if(!c.base){SymCleanup(process);return 2;}
    bool ok=SymEnumSymbols(process,c.base,"storage!*CFSFolder*GetSize*",found,&c)&&SymEnumSymbols(process,c.base,"storage!*CDefCollection*GetItemProperty*",found,&c)&&SymEnumSymbols(process,c.base,"storage!*CDefCollection*Item*GetValue*",found,&c);
    for(auto& s:c.symbols)if(!s.rva)ok=false;
    DWORD timestamp=0,size=0;ok=ok&&imageIdentity(storage,timestamp,size);
    auto ini=data+L"\\hooks.ini";
    if(ok){
        for(auto& s:c.symbols){auto n=std::to_wstring(s.rva);WritePrivateProfileStringW(L"Symbols",s.key,n.c_str(),ini.c_str());wprintf(L"%ls=%lu\n",s.key,s.rva);}
        WritePrivateProfileStringW(L"Symbols",L"TimeStamp",std::to_wstring(timestamp).c_str(),ini.c_str());
        WritePrivateProfileStringW(L"Symbols",L"ImageSize",std::to_wstring(size).c_str(),ini.c_str());
        log(L"Microsoft symbols resolved and cached");
    }else log(L"Required Explorer symbols unavailable; hooks disabled");
    SymCleanup(process);return ok?0:3;
}
