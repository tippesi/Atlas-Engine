#include "Worker.h"

namespace Atlas {

    Worker::Worker(int32_t workerId) : workerId(workerId) {

        

    }

    Worker::Worker(Worker&& worker) : workerId(worker.workerId) {


    }

    void Worker::Start(std::function<void(Worker&)> function) {

        thread = std::thread([this, function] { function(*this); });

    }

    void Worker::Work() {

        while(true) {
            auto job = queue.Pop();
            if (job == std::nullopt)
                break;

            RunJob(job.value());
        }

    }

    void Worker::RunJob(Job& job) {

        JobData data = {
            .idx = job.idx,
            .userData = job.userData
        };

        job.function(data);

        job.counter->fetch_sub(1);

    }

}