#define wWinMain setupEntryUnused
#include "setup.cpp"
#undef wWinMain
int wmain(){
    // This fixture is not the installed program. Its --stop intentionally
    // returns without stopping the original process, exercising the forced
    // termination/wait path before replacing a mapped executable.
    auto root=directory()+L"\\setup-fixture";auto exe=root+L"\\WindowsFolderSize.exe";
    std::wstring command=L"\""+exe+L"\"";STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 2;
    Sleep(100);bool replaced=false;
    try{stopAgent(root);if(WaitForSingleObject(pi.hProcess,0)!=WAIT_OBJECT_0)return 3;extract(101,exe);replaced=samePayload(101,exe);}
    catch(const std::exception& e){printf("FAIL: %s\n",e.what());TerminateProcess(pi.hProcess,0);WaitForSingleObject(pi.hProcess,5000);}
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    if(!replaced)return 4;DeleteFileW(exe.c_str());RemoveDirectoryW(root.c_str());
    puts("PASS forced stop fully releases mapped executable before payload replacement");return 0;
}
