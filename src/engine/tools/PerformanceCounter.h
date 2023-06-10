#ifndef AE_PERFORMANCECOUNTER_H
#define AE_PERFORMANCECOUNTER_H

#include "../System.h"

#include <vector>

namespace Atlas {

    namespace Tools {

        class PerformanceCounter {

        public:
            /**
             * Represents a time stamp.
             * @param delta The delta time in milliseconds to other stamp
             * @param stamp The stamp data (this is not a time)
             */
            struct TimeStamp {
                double delta = 0.0;
                double stamp = 0.0;
            };

            /**
             * Constructs a PerformanceCounter object.
             * @note The constructor also creates an initial TimeStamp.
             */
            PerformanceCounter();

            /**
             * Resets the performance counter to have a new initial TimeStamp.
             */
            void Reset();

            /**
             * Creates a new TimeStamp by using the time difference to the initial TimeStamp.
             * @return A new TimeStamp.
             */
            TimeStamp Stamp();

            /**
             * Creates a new TimeStamp by using the time difference to the input TimeStamp.
             * @param stamp A TimeStamp from which the difference to the new stamp is calculated.
             * @return A new TimeStamp.
             */
            TimeStamp Stamp(TimeStamp stamp);

            /**
             * Creates a new TimeStamp by using the time difference to the last created TimeStamp.
             * @return A new TimeStamp.
             */
            TimeStamp StepStamp();

            std::vector<TimeStamp> stamps;

        };

    }

}

#endif