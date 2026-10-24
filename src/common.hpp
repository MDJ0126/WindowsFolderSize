#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <shlobj.h>

namespace fsizes {
constexpr uint32_t magic = 0x46535A31;
constexpr uint32_t progressMagic = 0x46535A32;
constexpr wchar_t pipeName[] = L"\\\\.\\pipe\\WindowsFolderSize-";
struct Request { uint32_t magicValue = progressMagic; uint32_t characters = 0; };
struct LegacyResponse { uint32_t magicValue = magic; uint32_t ready = 0; uint64_t bytes = 0; };
enum class ScanState : uint32_t { queued=0, scanning=1, complete=2, failed=3, approximate=4 };
struct Response { uint32_t magicValue = progressMagic; uint32_t ready = 0; uint64_t bytes = 0; uint32_t percent=0; ScanState state=ScanState::queued; };
constexpr uint64_t approximateFlag=uint64_t(1)<<63;
inline bool approximateValue(uint64_t bytes){return (bytes&approximateFlag)&&bytes<UINT64_MAX-511;}
inline uint64_t plainSize(uint64_t bytes){return approximateValue(bytes)?bytes&~approximateFlag:bytes;}
inline uint64_t storedSize(uint64_t bytes,bool approximate){return approximate?bytes|approximateFlag:bytes;}
struct UiWords {const wchar_t* unavailable;const wchar_t* approximate;};
inline UiWords uiWords(LANGID language=GetUserDefaultUILanguage()){
 switch(PRIMARYLANGID(language)){
 case LANG_KOREAN:return {L"계산 불가",L"약 "};
 case LANG_JAPANESE:return {L"計算不可",L"約 "};
 case LANG_CHINESE:return SUBLANGID(language)==SUBLANG_CHINESE_TRADITIONAL||SUBLANGID(language)==SUBLANG_CHINESE_HONGKONG?UiWords{L"無法計算",L"約 "}:UiWords{L"无法计算",L"约 "};
 case LANG_GERMAN:return {L"Nicht verfügbar",L"Ca. "};
 case LANG_FRENCH:return {L"Indisponible",L"Env. "};
 case LANG_SPANISH:return {L"No disponible",L"Aprox. "};
 case LANG_PORTUGUESE:return {L"Indisponível",L"Aprox. "};
 case LANG_ITALIAN:return {L"Non disponibile",L"Circa "};
 case LANG_RUSSIAN:return {L"Недоступно",L"≈ "};
 case LANG_ARABIC:return {L"غير متاح",L"حوالي "};
 case LANG_DUTCH:return {L"Niet beschikbaar",L"Ca. "};
 case LANG_POLISH:return {L"Niedostępne",L"Około "};
 case LANG_TURKISH:return {L"Kullanılamıyor",L"Yaklaşık "};
 case LANG_SWEDISH:return {L"Ej tillgängligt",L"Cirka "};
 case LANG_CZECH:return {L"Nedostupné",L"Přibližně "};
 case LANG_UKRAINIAN:return {L"Недоступно",L"Прибл. "};
 case LANG_VIETNAMESE:return {L"Không khả dụng",L"Khoảng "};
 case LANG_THAI:return {L"ไม่พร้อมใช้งาน",L"ประมาณ "};
 case LANG_INDONESIAN:return {L"Tidak tersedia",L"Sekitar "};
 default:return {L"Unavailable",L"Approx. "};
 }
}
inline std::wstring progressText(const Response& response,unsigned dots=3){
    if(response.state==ScanState::failed)return uiWords().unavailable;
    return std::wstring(dots,L'.')+std::to_wstring(response.state==ScanState::queued?0u:(std::min)(response.percent,99u))+L"%";
}
// Keep System.Size canonical (VT_UI8) while a scan is pending. Reserve the
// top 512 UInt64 values, far beyond Windows volume/file size limits. Strings
// in System.Size are cleared by the property system's canonical conversion.
constexpr uint64_t pendingSizeBase=UINT64_MAX-511;
inline uint64_t pendingSize(const Response& response,unsigned frame=unsigned((GetTickCount64()/600)%3)){
    return pendingSizeBase+(frame%3)*128+(response.state==ScanState::failed?101:
        response.state==ScanState::queued?100:(std::min)(response.percent,99u));
}
inline bool pendingSizeText(uint64_t value,std::wstring& text){
    if(value<pendingSizeBase||value>pendingSizeBase+357)return false;
    unsigned encoded=unsigned(value-pendingSizeBase),code=encoded%128;if(code>101)return false;
    Response response;
    response.state=code==101?ScanState::failed:code==100?ScanState::queued:ScanState::scanning;
    response.percent=code;text=progressText(response,encoded/128+1);return true;
}
inline std::wstring directory() {
    wchar_t b[32768]; DWORD n = GetModuleFileNameW(nullptr,b,32768);
    std::wstring p(b,n); return p.substr(0,p.find_last_of(L"\\/"));
}
inline std::wstring dataDirectory() {
    // Program Files is read-only to the broker. Keep mutable symbols/logs
    // in the invoking user's known Local AppData folder, never beside code.
    PWSTR known=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&known)))return L"";
    std::wstring root=std::wstring(known)+L"\\WindowsFolderSize";CoTaskMemFree(known);
    wchar_t test[2];if(GetEnvironmentVariableW(L"FOLDERSIZE_TEST",test,2)>0)root+=L"-Tests";
    CreateDirectoryW(root.c_str(),nullptr);auto data=root+L"\\Data";CreateDirectoryW(data.c_str(),nullptr);return data;
}
inline std::wstring sessionPipe() {
    DWORD session=0; ProcessIdToSessionId(GetCurrentProcessId(),&session);
    wchar_t test[2];bool isolated=GetEnvironmentVariableW(L"FOLDERSIZE_TEST",test,2)>0;
    return std::wstring(pipeName)+(isolated?L"Test-":L"")+std::to_wstring(session);
}
inline void log(const wchar_t* message) {
    auto data=dataDirectory();if(data.empty())return;auto path=data+L"\\WindowsFolderSize.log";
    HANDLE h=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return;
    if(GetFileSize(h,nullptr)>1024*1024){CloseHandle(h);return;}
    SYSTEMTIME t;GetLocalTime(&t); wchar_t line[2048];
    int n=swprintf_s(line,L"%04u-%02u-%02u %02u:%02u:%02u %ls\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,message);
    if(n>0){DWORD written;WriteFile(h,line,DWORD(n*sizeof(wchar_t)),&written,nullptr);} CloseHandle(h);
}
inline bool imageIdentity(const std::wstring& path,DWORD& stamp,DWORD& size) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE)return false;
    IMAGE_DOS_HEADER dos{};DWORD n;bool ok=false;
    if(ReadFile(f,&dos,sizeof(dos),&n,nullptr)&&n==sizeof(dos)&&dos.e_magic==IMAGE_DOS_SIGNATURE){
        SetFilePointer(f,dos.e_lfanew,nullptr,FILE_BEGIN);IMAGE_NT_HEADERS64 pe{};
        if(ReadFile(f,&pe,sizeof(pe),&n,nullptr)&&n==sizeof(pe)&&pe.Signature==IMAGE_NT_SIGNATURE){stamp=pe.FileHeader.TimeDateStamp;size=pe.OptionalHeader.SizeOfImage;ok=true;}
    }CloseHandle(f);return ok;
}
inline std::wstring storagePath(){wchar_t p[MAX_PATH];GetSystemDirectoryW(p,MAX_PATH);return std::wstring(p)+L"\\windows.storage.dll";}
}
