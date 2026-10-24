#pragma once
#include "scanner.hpp"
#include <map>
namespace fsizes {
using FileSizes=std::map<std::wstring,uint64_t>;
inline std::wstring fileKey(std::wstring path){if(path.rfind(L"\\\\?\\",0)==0)path.erase(0,4);CharLowerBuffW(path.data(),DWORD(path.size()));return path;}
inline void removeFileTree(FileSizes& files,uint64_t& total,const std::wstring& path){auto exact=files.find(path);if(exact!=files.end()){total-=exact->second;files.erase(exact);}auto prefix=path+L"\\";auto it=files.lower_bound(prefix);while(it!=files.end()&&it->first.compare(0,prefix.size(),prefix)==0){total-=it->second;it=files.erase(it);}}
inline bool updateFileDelta(FileSizes& files,uint64_t& total,const std::wstring& path,DWORD action,const std::atomic<bool>& stop,bool* approximate=nullptr){
 WIN32_FILE_ATTRIBUTE_DATA data{};
 if(!GetFileAttributesExW(longPath(path).c_str(),GetFileExInfoStandard,&data)){DWORD error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return false;removeFileTree(files,total,path);return true;}
 if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
  // Directory timestamps change when children change; their own metadata
  // carries no byte delta. Only an added/moved-in subtree needs enumeration.
  if(action!=FILE_ACTION_ADDED&&action!=FILE_ACTION_RENAMED_NEW_NAME)return true;
  removeFileTree(files,total,path);if(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)return true;
  FileSizes subtree;ScanJob scan(path,stop,{},[&](const std::wstring& file,uint64_t bytes){subtree[fileKey(file)]=bytes;});while(!scan.step(512,8)){}if(!scan.value().complete)return false;if(approximate&&scan.value().approximate)*approximate=true;
  for(const auto& item:subtree){auto old=files.find(item.first);if(old!=files.end())total-=old->second;files[item.first]=item.second;total+=item.second;}return true;
 }
 uint64_t bytes=(uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow;auto old=files.find(path);if(old!=files.end())total-=old->second;files[path]=bytes;total+=bytes;return true;
}
}
