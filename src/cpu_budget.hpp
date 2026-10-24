#pragma once
#include "common.hpp"
namespace fsizes {
inline bool limitCpu(){
    HANDLE job=CreateJobObjectW(nullptr,nullptr);if(!job)return false;
    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION rate{};
    rate.ControlFlags=JOB_OBJECT_CPU_RATE_CONTROL_ENABLE|JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
    rate.CpuRate=100; // 1% of total system CPU, enforced by the scheduler.
    bool ok=SetInformationJobObject(job,JobObjectCpuRateControlInformation,&rate,sizeof(rate))&&AssignProcessToJobObject(job,GetCurrentProcess());
    if(!ok){CloseHandle(job);return false;}
    // Keep the job alive for this broker lifetime. No kill-on-close policy.
    static HANDLE budget=job;(void)budget;SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);return true;
}
}
