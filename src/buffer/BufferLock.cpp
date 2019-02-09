#include "BufferLock.h"

namespace Atlas {

    namespace Buffer {

        BufferLock::BufferLock() {



        }

        BufferLock::~BufferLock() {

            for (auto& lock : locks)
                Cleanup(lock);

            locks.clear();

        }

        void BufferLock::WaitForLockedRange(size_t offset, size_t length) {

            struct LockRange lockRange = { offset, length };

            std::vector<Lock> swapLocks;

            for (auto& lock : locks) {
                if(lockRange.Overlaps(lock.range)) {
                    Wait(lock.sync);
                    Cleanup(lock);
                }
                else {
                    swapLocks.push_back(lock);
                }
            }

            locks.swap(swapLocks);

        }

        void BufferLock::LockRange(size_t offset, size_t length) {

            struct LockRange lockRange = { offset, length };
            GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

            Lock lock = { lockRange, sync };

            locks.push_back(lock);

        }

        void BufferLock::Wait(GLsync &sync) {

            uint32_t waitFlags = 0;
            uint64_t waitDuration = 0;

            while (true) {
                uint32_t waitRet = glClientWaitSync(sync, waitFlags, waitDuration);
                if (waitRet == GL_ALREADY_SIGNALED || waitRet == GL_CONDITION_SATISFIED) {
                    return;
                }

                if (waitRet == GL_WAIT_FAILED) {
                    throw AtlasException("Failed to access memory");
                }

                // Time is in nanoseconds (we might need to change the wait duration)
                waitFlags = GL_SYNC_FLUSH_COMMANDS_BIT;
                waitDuration = 1000000000;

            }

        }

        void BufferLock::Cleanup(BufferLock::Lock &lock) {

            glDeleteSync(lock.sync);

        }

    }

}