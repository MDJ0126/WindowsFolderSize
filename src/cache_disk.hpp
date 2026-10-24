#pragma once
#include "common.hpp"
#include <map>
namespace fsizes {
using SavedSizes=std::map<std::wstring,uint64_t>;
inline SavedSizes loadSizes(const std::wstring& path){
 SavedSizes result;HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return result;
 DWORD size=GetFileSize(file,nullptr),n=0;if(size<12||size>32*1024*1024){CloseHandle(file);return result;}std::vector<BYTE> data(size);bool ok=ReadFile(file,data.data(),size,&n,nullptr)&&n==size;CloseHandle(file);if(!ok)return result;
 size_t offset=0;auto read=[&](void* value,size_t bytes){if(bytes>data.size()-offset)return false;memcpy(value,data.data()+offset,bytes);offset+=bytes;return true;};uint32_t magicValue=0,version=0,count=0;
 if(!read(&magicValue,4)||!read(&version,4)||!read(&count,4)||magicValue!=0x57465343||(version!=1&&version!=2)||count>4096)return {};
 for(uint32_t i=0;i<count;i++){uint32_t length=0;uint64_t bytes=0;if(!read(&length,4)||!read(&bytes,8)||length<3||length>=32000||length*2>data.size()-offset)return {};std::wstring name(length,L'\0');if(!read(name.data(),length*2)||name[1]!=L':'||name[2]!=L'\\'||name.find(L'\0')!=std::wstring::npos)return {};result[name]=bytes;}
 if(offset!=data.size())return {};return result;
}
inline bool saveSizes(const std::wstring& path,const SavedSizes& sizes){
 std::vector<BYTE> data;auto add=[&](const void* value,size_t bytes){auto p=static_cast<const BYTE*>(value);data.insert(data.end(),p,p+bytes);};uint32_t magicValue=0x57465343,version=2,count=uint32_t((std::min)(sizes.size(),size_t(4096)));add(&magicValue,4);add(&version,4);add(&count,4);
 uint32_t emitted=0;for(const auto& item:sizes){if(emitted++>=count)break;uint32_t length=uint32_t(item.first.size());add(&length,4);add(&item.second,8);add(item.first.data(),length*2);if(data.size()>32*1024*1024)return false;}
 auto temporary=path+L".tmp";HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;DWORD n=0;bool ok=WriteFile(file,data.data(),DWORD(data.size()),&n,nullptr)&&n==data.size()&&FlushFileBuffers(file);CloseHandle(file);if(!ok)return false;return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
}
