#pragma once
#include "common.hpp"
#include <atomic>
#include <functional>

namespace fsizes {
struct ScanResult { uint64_t bytes=0, files=0, directories=0; DWORD error=0; bool complete=false; };
inline std::wstring longPath(const std::wstring& p) {
    if(p.rfind(L"\\\\?\\",0)==0)return p;
    if(p.rfind(L"\\\\",0)==0)return L"\\\\?\\UNC\\"+p.substr(2);
    return L"\\\\?\\"+p;
}
using ProgressCallback=std::function<void(unsigned,uint64_t)>;
// A resumable directory enumeration lets one low-priority worker share disk
// time across visible folders. An expensive folder no longer blocks every
// subsequent folder at queued/0% until its entire tree has finished.
class ScanJob {
    struct Task {std::wstring path;double weight;};
    std::vector<Task> stack;
    Task current;
    std::vector<std::wstring> children;
    HANDLE find=INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data{};
    double completedWeight=0;
    const std::atomic<bool>& stopping;
    ProgressCallback progress;
    std::function<void(const std::wstring&,uint64_t)> observe;
    ScanResult result;
    bool finished=false;
    void report(){if(progress)progress((std::min)(99u,unsigned(completedWeight*100)),result.files);}
    void fail(DWORD error){result.error=error;finished=true;if(find!=INVALID_HANDLE_VALUE){FindClose(find);find=INVALID_HANDLE_VALUE;}}
    void finishDirectory(){double share=current.weight/double(children.size()+1);completedWeight+=share;for(auto& child:children)stack.push_back({std::move(child),share});children.clear();report();}
public:
    ScanJob(const std::wstring& root,const std::atomic<bool>& stop,ProgressCallback callback={},std::function<void(const std::wstring&,uint64_t)> observer={}):stopping(stop),progress(std::move(callback)),observe(std::move(observer)){
        if(progress)progress(0,0);DWORD attrs=GetFileAttributesW(longPath(root).c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES){fail(GetLastError());return;}
        if(!(attrs&FILE_ATTRIBUTE_DIRECTORY)){fail(ERROR_DIRECTORY);return;}
        if(attrs&FILE_ATTRIBUTE_REPARSE_POINT){fail(ERROR_REPARSE_TAG_INVALID);return;}
        stack.push_back({longPath(root),1.0});
    }
    ScanJob(const ScanJob&)=delete;
    ~ScanJob(){if(find!=INVALID_HANDLE_VALUE)FindClose(find);}
    // Count entries as well as elapsed time so flat directories yield too.
    bool step(unsigned entries=128,DWORD milliseconds=8){
        auto start=GetTickCount64();unsigned processed=0;
        while(!finished){
            if(stopping.load()){fail(ERROR_CANCELLED);break;}
            if(find==INVALID_HANDLE_VALUE){
                if(stack.empty()){result.complete=true;finished=true;if(progress)progress(100,result.files);break;}
                current=std::move(stack.back());stack.pop_back();++result.directories;
                find=FindFirstFileExW((current.path+L"\\*").c_str(),FindExInfoBasic,&data,FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);
                if(find==INVALID_HANDLE_VALUE){DWORD e=GetLastError();if(e!=ERROR_FILE_NOT_FOUND){fail(e);break;}completedWeight+=current.weight;report();if(++processed>=entries||GetTickCount64()-start>=milliseconds)break;continue;}
            }
            if(!(data.cFileName[0]==L'.'&&(data.cFileName[1]==0||(data.cFileName[1]==L'.'&&data.cFileName[2]==0)))){
                if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))children.push_back(current.path+L"\\"+data.cFileName);}
                else{uint64_t bytes=(uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow;result.bytes+=bytes;++result.files;if(observe)observe(current.path+L"\\"+data.cFileName,bytes);}
            }
            if(!FindNextFileW(find,&data)){DWORD e=GetLastError();FindClose(find);find=INVALID_HANDLE_VALUE;if(e!=ERROR_NO_MORE_FILES){fail(e);break;}finishDirectory();}
            if(++processed>=entries||GetTickCount64()-start>=milliseconds)break;
        }
        return finished;
    }
    const ScanResult& value()const{return result;}
};
inline ScanResult scan(const std::wstring& root,const std::atomic<bool>& stopping,const ProgressCallback& progress={}){
    ScanJob job(root,stopping,progress);while(!job.step()){}return job.value();
}
}
