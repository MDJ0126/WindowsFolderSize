#pragma once
#include "scanner.hpp"
#include <map>
namespace fsizes {
using FileSizes=std::map<std::wstring,uint64_t>;
struct IndexChange {std::wstring path;uint64_t bytes=0;bool remove=false;};
using IndexChanges=std::vector<IndexChange>;
inline std::wstring fileKey(std::wstring path){if(path.rfind(L"\\\\?\\",0)==0)path.erase(0,4);CharLowerBuffW(path.data(),DWORD(path.size()));return path;}
inline void removeFileTree(FileSizes& files,uint64_t& total,const std::wstring& path){auto exact=files.find(path);if(exact!=files.end()){total-=exact->second;files.erase(exact);}auto prefix=path+L"\\";auto it=files.lower_bound(prefix);while(it!=files.end()&&it->first.compare(0,prefix.size(),prefix)==0){total-=it->second;it=files.erase(it);}}
inline bool moveFileTree(FileSizes& files,uint64_t& total,const std::wstring& oldPath,const std::wstring& newPath,IndexChanges& journal){
 FileSizes moved;auto exact=files.find(oldPath);if(exact!=files.end())moved[newPath]=exact->second;auto prefix=oldPath+L"\\";
 for(auto it=files.lower_bound(prefix);it!=files.end()&&it->first.compare(0,prefix.size(),prefix)==0;++it)moved[newPath+it->first.substr(oldPath.size())]=it->second;
 if(moved.empty())return false;
 removeFileTree(files,total,oldPath);removeFileTree(files,total,newPath);journal.push_back({oldPath,0,true});journal.push_back({newPath,0,true});
 for(const auto& f:moved){files[f.first]=f.second;total+=f.second;journal.push_back({f.first,f.second,false});}return true;
}
inline bool updateFileDelta(FileSizes& files,uint64_t& total,const std::wstring& path,DWORD action,const std::atomic<bool>& stop,bool* approximate=nullptr,IndexChanges* journal=nullptr){
 WIN32_FILE_ATTRIBUTE_DATA data{};
 if(!GetFileAttributesExW(longPath(path).c_str(),GetFileExInfoStandard,&data)){DWORD error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return false;removeFileTree(files,total,path);if(journal)journal->push_back({path,0,true});return true;}
 if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
  // Directory timestamps change when children change; their own metadata
  // carries no byte delta. Only an added/moved-in subtree needs enumeration.
  if(action!=FILE_ACTION_ADDED&&action!=FILE_ACTION_RENAMED_NEW_NAME)return true;
  removeFileTree(files,total,path);if(journal)journal->push_back({path,0,true});if(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)return true;
  FileSizes subtree;ScanJob scan(path,stop,{},[&](const std::wstring& file,uint64_t bytes){subtree[fileKey(file)]=bytes;});while(!scan.step(512,8)){}if(!scan.value().complete)return false;if(approximate&&scan.value().approximate)*approximate=true;
  for(const auto& item:subtree){auto old=files.find(item.first);if(old!=files.end())total-=old->second;files[item.first]=item.second;total+=item.second;if(journal)journal->push_back({item.first,item.second,false});}return true;
 }
 uint64_t bytes=(uint64_t(data.nFileSizeHigh)<<32)|data.nFileSizeLow;auto old=files.find(path);if(old!=files.end())total-=old->second;files[path]=bytes;total+=bytes;if(journal)journal->push_back({path,bytes,false});return true;
}
// If a deleted ancestor is in the same batch, its cached range accounts for
// every descendant. Avoid one metadata lookup per removed file.
inline std::map<std::wstring,DWORD> compactChanges(const std::wstring& root,const std::map<std::wstring,DWORD>& changes){
 std::map<std::wstring,DWORD> result;std::wstring removedPrefix;
 for(const auto& c:changes){auto path=fileKey(root+L"\\"+c.first);if(!removedPrefix.empty()&&path.compare(0,removedPrefix.size(),removedPrefix)==0)continue;
  result[c.first]=c.second;DWORD attrs=GetFileAttributesW(longPath(path).c_str());if(attrs==INVALID_FILE_ATTRIBUTES){auto e=GetLastError();if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)removedPrefix=path+L"\\";}
 }return result;
}
}
