#include "file_index.hpp"
#include <filesystem>
using namespace fsizes;
int wmain(){
 SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");auto root=fileKey(directory()+L"\\index-fixture-"+std::to_wstring(GetCurrentProcessId()));FileSizes files;
 for(unsigned i=0;i<100000;i++)files[root+L"\\large\\file-"+std::to_wstring(i)]=i+1;
 files[root+L"\\keep.bin"]=17;uint64_t total=5000050017ull;auto start=GetTickCount64();
 if(!writeIndex(root,files,total,false))return 2;FileSizes restored;uint64_t restoredTotal=0;bool approximate=false;
 if(!readIndex(root,restored,restoredTotal,approximate)||restored!=files||restoredTotal!=total||approximate)return 3;
 IndexChanges journal;if(!moveFileTree(files,total,root+L"\\large",root+L"\\renamed",journal)||total!=restoredTotal)return 4;
 if(!writeIndex(root,files,total,false,&journal)||!readIndex(root,restored,restoredTotal,approximate)||restored!=files)return 5;
 journal.clear();removeFileTree(files,total,root+L"\\renamed");journal.push_back({root+L"\\renamed",0,true});
 if(total!=17||!writeIndex(root,files,total,true,&journal)||!readIndex(root,restored,restoredTotal,approximate)||restored!=files||restoredTotal!=17||!approximate)return 6;
 // A torn append must never be accepted as a complete, current index.
 HANDLE h=CreateFileW(indexPath(root).c_str(),FILE_APPEND_DATA,0,nullptr,OPEN_EXISTING,0,nullptr);BYTE torn=0x19;DWORD n;WriteFile(h,&torn,1,&n,nullptr);CloseHandle(h);
 if(readIndex(root,restored,restoredTotal,approximate))return 7;
 if(!writeIndex(root,files,total,false)||!readIndex(root,restored,restoredTotal,approximate)||restoredTotal!=17)return 8;
 DeleteFileW(indexPath(root).c_str());printf("PASS durable 100000-file index: snapshot, cached subtree rename, deletion journal, exact totals, partial flag, torn-append rejection and recovery (%llu ms)\n",GetTickCount64()-start);return 0;
}
