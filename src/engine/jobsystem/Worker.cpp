#include "Worker.h"
#include "../Log.h"

#ifdef AE_OS_WINDOWS
#include "Windows.h"
#endif

#ifdef AE_OS_MACOS
#include <pthread.h>
#endif

#ifdef AE_OS_LINUX
#include <algorithm>
#include <cerrno>
#include <sys/resource.h>
#endif

namespace Atlas {

    Worker::Worker(int32_t workerId, JobPriority priority) : workerId(workerId), priority(priority) {

        

    }

    Worker::Worker(Worker&& worker) : workerId(worker.workerId), priority(worker.priority) {


    }

    void Worker::Start(std::function<void(Worker&)> function) {

        thread = std::thread([this, function] {
            // The priority is set before the first job runs. All priorities stay in the normal
            // scheduling range, such that system threads (e.g. audio) are never starved
            bool success = true;
#ifdef AE_OS_WINDOWS
            auto threadHandle = GetCurrentThread();
            switch (priority) {
                case JobPriority::High: success = SetThreadPriority(threadHandle, THREAD_PRIORITY_HIGHEST) > 0; break;
                case JobPriority::Low: success = SetThreadPriority(threadHandle, THREAD_PRIORITY_LOWEST) > 0; break;
                default: success = SetThreadPriority(threadHandle, THREAD_PRIORITY_NORMAL) > 0; break;
            }
            std::wstring threadName = L"Worker " + std::to_wstring(workerId) + L" priority " + std::to_wstring(static_cast<int>(priority));
            SetThreadDescription(threadHandle, threadName.c_str());
#endif
#ifdef AE_OS_MACOS
            switch (priority) {
                case JobPriority::High: success = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0) == 0; break;
                case JobPriority::Low: success = pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0) == 0; break;
                default: success = pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0) == 0; break;
            }
#endif
#ifdef AE_OS_LINUX
            // Linux nice values are per thread. Only lower worker priorities so no privileges are needed.
            const int niceOffset = priority == JobPriority::High ? 0 : priority == JobPriority::Low ? 10 : 5;
            errno = 0;
            const int inheritedNice = getpriority(PRIO_PROCESS, 0);
            if (errno != 0)
                success = false;
            else if (niceOffset > 0)
                success &= setpriority(PRIO_PROCESS, 0, std::min(19, inheritedNice + niceOffset)) == 0;
#endif
            if (!success)
                Log::Warning("Couldn't set thread priority");

            function(*this);
        });

    }

}
