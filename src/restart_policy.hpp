#pragma once
#include <cstdint>
// A failed RmRestart must never bypass shell recovery. A changed shell PID
// also prevents an old shell still shutting down from being treated as ready.
template<class ShellPid,class Launch,class Pause>
unsigned restoreDesktop(uint32_t previous,ShellPid shellPid,Launch launch,Pause pause){
    unsigned stable=0,launches=0;
    for(unsigned tick=0;tick<150;++tick){
        uint32_t current=shellPid();
        if(current&&current!=previous){if(++stable>=10)return 0;}
        else {stable=0;if(!current&&(tick%25==0)&&launches<5){++launches;launch();}}
        pause();
    }
    return 1460;
}
