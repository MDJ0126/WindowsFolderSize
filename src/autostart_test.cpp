#include "common.hpp"
using namespace fsizes;
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;SetEnvironmentVariableW(L"FOLDERSIZE_TEST",L"1");CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IShellFolder* desktop=nullptr;SHGetDesktopFolder(&desktop);if(desktop)desktop->Release();LoadLibraryW(L"windows.storage.dll");
 auto dll=LoadLibraryW(argv[1]);if(!dll)return 3;auto status=reinterpret_cast<LONG(*)()>(GetProcAddress(dll,"WindowsFolderSizeHookStatus"));if(!status)return 4;
 // Deliberately do not call WindowsFolderSizeInitialize or DllGetClassObject.
 for(unsigned i=0;i<200&&status()==0;i++)Sleep(10);printf("automatic initialization without COM factory or context menu: status=%ld\n",status());return status()==1?0:5;
}
