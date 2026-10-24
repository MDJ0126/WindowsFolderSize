#pragma once
#include "file_delta.hpp"
namespace fsizes {
inline uint64_t indexHash(const void* data,size_t n){uint64_t h=14695981039346656037ull;auto p=static_cast<const BYTE*>(data);while(n--){h^=*p++;h*=1099511628211ull;}return h;}
inline std::wstring indexPath(const std::wstring& root){wchar_t name[40];swprintf_s(name,L"\\index-%016llx.bin",indexHash(root.data(),root.size()*2));return dataDirectory()+name;}
// Relative paths keep the durable per-file index compact. Each committed block
// is checksummed; an interrupted append is rejected and triggers recovery.
inline bool writeIndex(const std::wstring& root,const FileSizes& files,uint64_t total,bool approximate,const IndexChanges* changes=nullptr){
 std::vector<BYTE> body;auto add=[&](const void* p,size_t n){auto b=static_cast<const BYTE*>(p);body.insert(body.end(),b,b+n);};
 uint32_t rootLength=uint32_t(root.size()),count=uint32_t(changes?changes->size():files.size());uint64_t stored=storedSize(total,approximate);add(&rootLength,4);add(root.data(),root.size()*2);add(&stored,8);add(&count,4);
 auto record=[&](const std::wstring& path,uint64_t bytes,bool remove){if(path.size()<=root.size()||path.compare(0,root.size(),root)!=0||path[root.size()]!=L'\\')return false;auto relative=path.substr(root.size()+1);uint32_t length=uint32_t(relative.size());BYTE op=remove?1:0;add(&op,1);add(&length,4);add(&bytes,8);add(relative.data(),relative.size()*2);return true;};
 if(changes){for(const auto& c:*changes)if(!record(c.path,c.bytes,c.remove))return false;}else for(const auto& f:files)if(!record(f.first,f.second,false))return false;
 if(body.size()>512ull*1024*1024)return false;
 auto path=indexPath(root),target=changes?path:path+L".tmp";
 if(changes&&changes->empty())return true;
 HANDLE h=CreateFileW(target.c_str(),changes?FILE_APPEND_DATA:GENERIC_WRITE,FILE_SHARE_READ,nullptr,changes?OPEN_EXISTING:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER existing{};if(changes&&(!GetFileSizeEx(h,&existing)||existing.QuadPart>64ll*1024*1024)){CloseHandle(h);return false;}
 uint32_t magicValue=0x57464931,length=uint32_t(body.size());uint64_t hash=indexHash(body.data(),body.size());DWORD n;bool ok=true;auto write=[&](const void* p,DWORD size){if(!WriteFile(h,p,size,&n,nullptr)||n!=size)ok=false;};
 if(!changes)write(&magicValue,4);write(&length,4);write(&hash,8);write(body.data(),length);if(!FlushFileBuffers(h))ok=false;CloseHandle(h);
 if(!ok)return false;return changes||MoveFileExW(target.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
inline bool readIndex(const std::wstring& root,FileSizes& output,uint64_t& total,bool& approximate){
 HANDLE h=CreateFileW(indexPath(root).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};DWORD n;uint32_t magicValue=0;bool ok=GetFileSizeEx(h,&size)&&size.QuadPart>=4&&size.QuadPart<=1024ll*1024*1024&&ReadFile(h,&magicValue,4,&n,nullptr)&&n==4&&magicValue==0x57464931;uint64_t consumed=4,sum=0;FileSizes files;bool approx=false;
 while(ok&&consumed<uint64_t(size.QuadPart)){
  uint32_t length=0;uint64_t hash=0;ok=ReadFile(h,&length,4,&n,nullptr)&&n==4&&ReadFile(h,&hash,8,&n,nullptr)&&n==8;consumed+=12;if(!ok||length>512u*1024*1024||length>uint64_t(size.QuadPart)-consumed){ok=false;break;}
  std::vector<BYTE> data(length);ok=ReadFile(h,data.data(),length,&n,nullptr)&&n==length&&indexHash(data.data(),data.size())==hash;consumed+=length;if(!ok)break;
  size_t at=0;auto read=[&](void* p,size_t bytes){if(bytes>data.size()-at)return false;memcpy(p,data.data()+at,bytes);at+=bytes;return true;};uint32_t rootLength=0,count=0;uint64_t stored=0;
  if(!read(&rootLength,4)||rootLength!=root.size()){ok=false;break;}std::wstring savedRoot(rootLength,L'\0');if(!read(savedRoot.data(),rootLength*2)||savedRoot!=root||!read(&stored,8)||!read(&count,4)||count>5000000){ok=false;break;}
  for(uint32_t i=0;i<count&&ok;i++){BYTE op;uint32_t chars;uint64_t bytes;if(!read(&op,1)||op>1||!read(&chars,4)||!read(&bytes,8)||!chars||chars>=32000||chars*2>data.size()-at){ok=false;break;}std::wstring relative(chars,L'\0');read(relative.data(),chars*2);if(relative.find(L'\0')!=std::wstring::npos||relative.front()==L'\\'||relative.find(L"..\\")!=std::wstring::npos){ok=false;break;}auto path=root+L"\\"+relative;if(op)removeFileTree(files,sum,path);else{auto old=files.find(path);if(old!=files.end())sum-=old->second;files[path]=bytes;sum+=bytes;}}
  if(at!=data.size()||sum!=plainSize(stored))ok=false;approx=approximateValue(stored);
 }
 CloseHandle(h);if(!ok||consumed!=uint64_t(size.QuadPart))return false;output=std::move(files);total=sum;approximate=approx;return true;
}
}
