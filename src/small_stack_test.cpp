#include <windows.h>
#include <cstdio>
DWORD WINAPI load(void* p){auto m=LoadLibraryW((wchar_t*)p);auto e=GetLastError();printf("load=%p error=%lu\n",m,e);return m?0:e;}
int wmain(int n,wchar_t** v){if(n!=2)return 3;auto t=CreateThread(nullptr,65536,load,v[1],STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);if(!t)return 4;WaitForSingleObject(t,INFINITE);DWORD result;GetExitCodeThread(t,&result);printf("64 KiB loader thread result=%lu\n",result);return result;}

