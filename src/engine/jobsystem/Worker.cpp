#include "Worker.h"
#include "../Log.h"

#ifdef AE_OS_WINDOWS
#include "Windows.h"
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
        case JobPriority::Low: success &= SetThreadPriority(threadHandle, THREAD_PRIORITY_LOWEST); break;
        default: break;
        }       
#endif

        if (!success)
            Log::Warning("Couldn't set thread priority");

    }

}