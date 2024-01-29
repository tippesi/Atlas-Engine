#pragma once

#include "../System.h"

namespace Atlas {

    namespace Events {

        /**
          * A class to distribute frame update events.
          * @note The clock event gets fired before any other
          * event in a frame.
         */
        class FrameEvent {

        public:
            explicit FrameEvent(float deltaTime) {

                this->deltaTime = deltaTime;

            }

            /**
             * The time delta between the last event and this.
             */
            float deltaTime;

        };

    }

}