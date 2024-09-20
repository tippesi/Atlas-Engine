#include "Worker.h"
#include "../Log.h"

#ifdef AE_OS_WINDOWS
#include "Windows.h"
#endif

#if defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <pthread.h>
#endif

namespace Atlas {

    Worker::Worker(int32_t workerId, JobPriority priority) : workerId(workerId), priority(priority) {

        

    }

    Worker::Worker(Worker&& worker) : workerId(worker.workerId), priority(worker.priority) {


    }

    void Worker::Start(std::function<void(Worker&)> function) {

        thread = std::thread([this, function] { function(*this); });

        bool success = true;
#ifdef AE_OS_WINDOWS
        HANDLE threadHandle = static_cast<HANDLE>(thread.native_handle());
        switch (priority) {
            case JobPriority::High: success &= SetThreadPriority(threadHandle, THREAD_PRIORITY_HIGHEST); break;
            case JobPriority::Low: success &= SetThreadPriority(threadHandle, THREAD_BASE_PRIORITY_IDLE); break;
            default: success &= SetThreadPriority(threadHandle, THREAD_BASE_PRIORITY_IDLE); break; 
        }
        std::wstring threadName = L"Atlas worker " + std::to_wstring(workerId) + L" priority " + std::to_wstring(static_cast<int>(priority));
        SetThreadDescription(
            threadHandle,
            threadName.c_str()
        );
#endif
#if defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
        auto minPriority = sched_get_priority_min(SCHED_RR);
        auto maxPriority = sched_get_priority_max(SCHED_RR);

        sched_param params = {};
        
        switch (priority) {
            case JobPriority::High: params.sched_priority = maxPriority; break;
            case JobPriority::Low: params.sched_priority = minPriority; break;
            default: params.sched_priority = (maxPriority + minPriority) / 2; break;
        }
        success = pthread_setschedparam(thread.native_handle(), SCHED_RR, &params) == 0;
#endif
        if (!success)
            Log::Warning("Couldn't set thread priority");

    }

}
