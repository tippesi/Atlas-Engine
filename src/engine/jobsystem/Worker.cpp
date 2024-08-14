#include "Worker.h"

namespace Atlas {

    Worker::Worker(int32_t workerId) : workerId(workerId) {

        

    }

    Worker::Worker(Worker&& worker) : workerId(worker.workerId) {


    }

    void Worker::Start(std::function<void(Worker&)> function) {

        thread = std::thread([this, function] { function(*this); });

    }

}