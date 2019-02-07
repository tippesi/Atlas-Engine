#ifndef AE_BUFFERLOCK_H
#define AE_BUFFERLOCK_H

#include "../System.h"

#include <vector>

namespace Atlas {

    namespace Buffer {

        /**
         * Manages the memory safety of shared memory between driver and engine.
         * Thanks to John McDonald: https://github.com/nvMcJohn/apitest
         */
        class BufferLock {

        public:
            BufferLock();

            /**
             * Waits until the range is free to use again.
             * @param offset The offset in bytes
             * @param length The length in bytes
             * @note To improve this use multiple buffering
             */
            void WaitForLockedRange(size_t offset, size_t length);

            /**
             * Locks the range
             * @param offset The offset in bytes
             * @param length The length in bytes
             */
            void LockRange(size_t offset, size_t length);

            ~BufferLock();

        private:
            /**
             * Represents a part of the memory which is locked
             */
            struct LockRange {

                size_t offset;
                size_t length;

                bool Overlaps(LockRange& range) {
                    return offset < range.offset + range.length
                           && range.offset < offset + length;
                }

            };

            /**
             * A lock for a specific range
             */
            struct Lock {

                struct LockRange range;
                GLsync sync;

            };

            /**
             * Waits until the fence is reached by the driver
             * @param sync The OpenGL sync object.
             */
            void Wait(GLsync& sync);

            /**
             * Cleans up the lock and fence.
             * @param lock The lock to be cleaned up
             */
            void Cleanup(Lock& lock);

            std::vector<Lock> locks;

        };

    }

}

#endif
