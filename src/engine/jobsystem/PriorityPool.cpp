#include "PriorityPool.h"

namespace Atlas {

    void PriorityPool::Init(int32_t workerCount, JobPriority priority) {

        this->workerCount = workerCount;
        this->priority = priority;
        this->spinCounter = workerCount;

        for (int32_t i = 0; i < workerCount; i++) {
            workers.emplace_back(i);
        }

        for (int32_t i = 0; i < workerCount; i++) {
            workers[i].Start([&](Worker& worker) {
                while (!shutdown) {
                    worker.semaphore.acquire();
                    Work(worker.workerId);
                }
                });
        }

    }

    void PriorityPool::Shutdown() {

        shutdown = true;
        for (auto& worker : workers) {
            worker.semaphore.release();
            worker.thread.join();
        }

    }

    void PriorityPool::Work(int32_t workerId) {

        auto& worker = workers[workerId];
        worker.Work();

        for (int32_t i = 1; i < workerCount; i++) {
            auto stealIdx = (i + workerId) % workerCount;
            auto& stealFrom = workers[stealIdx];
            stealFrom.Work();
        }

    }

    Worker& PriorityPool::GetNextWorker() {

        auto counter = workerCounter++;
        auto workedId = counter % workerCount;
        return workers[workedId];

    }

    std::vector<Worker>& PriorityPool::GetAllWorkers() {

        return workers;

    }

}