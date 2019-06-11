#ifndef AE_CLOCKEVENT_H
#define AE_CLOCKEVENT_H

#include "../System.h"

namespace Atlas {

    namespace Events {

        /**
          * A class to distribute frame update events.
		  * @note The clock event gets fired before any other
		  * event in a frame.
         */
        class ClockEvent {

        public:
            ClockEvent(float dtime) {

                this->dtime = dtime;

            }

            /**
             * The time delta between the last event and this.
             */
            float dtime;

        };

    }

}

#endif