#include "restart_policy.hpp"
#include <cstdio>
int main(){
 unsigned ticks=0,launches=0; uint32_t pid=0;
 auto error=restoreDesktop(10,[&]{return pid;},[&]{++launches;pid=20;return true;},[&]{++ticks;});
 if(error||launches!=1||ticks<10)return 1;
 ticks=0;launches=0;pid=10;
 error=restoreDesktop(10,[&]{if(ticks==4)pid=0;return pid;},[&]{++launches;pid=30;return true;},[&]{++ticks;});
 if(error||launches!=1)return 2;
 ticks=0;launches=0;
 error=restoreDesktop(10,[]{return 40u;},[&]{++launches;return true;},[&]{++ticks;});
 if(error||launches)return 3;
 ticks=0;launches=0;
 error=restoreDesktop(10,[]{return 0u;},[&]{++launches;return false;},[&]{++ticks;});
 if(error!=1460||launches!=5||ticks!=150)return 4;
 puts("PASS: restart failure recovery, old-shell transition, existing desktop, bounded launch failure");return 0;
}
