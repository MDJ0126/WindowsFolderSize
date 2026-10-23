#include "common.hpp"
#include <filesystem>
using namespace fsizes;
bool read(const std::wstring& path,Response& r){Request req;req.characters=uint32_t(path.size());std::vector<BYTE> data(sizeof(req)+path.size()*2);memcpy(data.data(),&req,sizeof(req));memcpy(data.data()+sizeof(req),path.data(),path.size()*2);DWORD n=0;return CallNamedPipeW(sessionPipe().c_str(),data.data(),DWORD(data.size()),&r,sizeof(r),&n,1000)&&n==sizeof(r)&&r.magicValue==progressMagic;}
int wmain(){
    auto path=directory()+L"\\cache-fixture-"+std::to_wstring(GetCurrentProcessId());std::filesystem::create_directory(path);auto file=path+L"\\changing.bin";
    HANDLE h=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return 2;
    char data[10]{};DWORD n;WriteFile(h,data,10,&n,nullptr);FlushFileBuffers(h);CloseHandle(h);
    Response r;bool initial=false;for(int i=0;i<100;i++){if(read(path,r)&&r.ready&&r.bytes==10){initial=true;break;}Sleep(100);}if(!initial)return 3;
    bool lost=false;unsigned changes=0;
    for(unsigned i=0;i<60;i++){
        if(i%2==0){h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return 4;WriteFile(h,data,1,&n,nullptr);FlushFileBuffers(h);CloseHandle(h);changes++;}
        if(read(path,r)&&!r.ready)lost=true;Sleep(100);
    }
    bool final=false;for(int i=0;i<120;i++){if(read(path,r)&&r.ready&&r.bytes==10+changes){final=true;break;}Sleep(100);}
    std::filesystem::remove(file);bool deleted=false;
    for(int i=0;i<120;i++){if(read(path,r)&&r.ready&&r.bytes==0){deleted=true;break;}Sleep(100);}
    std::filesystem::remove(path);
    printf("continuous writes: snapshot_lost=%d final_correct=%d deletion_correct=%d\n",lost,final,deleted);
    if(lost)return 5;if(!final||!deleted)return 6;
    puts("PASS completed size remains visible during changes; final size and deletion refresh");return 0;
}
