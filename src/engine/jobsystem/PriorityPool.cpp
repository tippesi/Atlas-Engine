#include "PriorityPool.h"

namespace Atlas {

    void PriorityPool::Init(int32_t workerCount, JobPriority priority) {

        this->workerCount = workerCount;
        this->priority = priority;
        this->spinCounter = workerCount;

        for (int32_t i = 0; i < workerCount; i++) {
            workers.emplace_back(i, priority);
        }

        for (int32_t i = 0; i < workerCount; i++) {
            workers[i].Start([&](Worker& worker) {
                while (!shutdown) {
                    worker.signal.Wait();

                    Work(worker.workerId);
                }
                worker.quit = true;
                });
        }

    }

    void PriorityPool::Shutdown() {

        shutdown = true;
        for (auto& worker : workers) {
            // Try to get it to quit
            while(!worker.quit) {
                worker.signal.Notify();
                std::this_thread::yield();
            }
            worker.thread.join();
        }

    }

    void PriorityPool::Work(int32_t workerId) {

        auto& worker = workers[workerId];
        worker.Work();

        for (int32_t i = 1; i < workerCount; i++) {
            auto stealIdx = (-i + workerId + workerCount) % workerCount;
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
