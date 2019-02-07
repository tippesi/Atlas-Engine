#ifndef AE_TOUCH_H
#define AE_TOUCH_H

#include "../System.h"
#include "events/EventManager.h"

namespace Atlas {

    namespace Input {

        class TouchHandler {

        public:


        private:
            void TouchEventHandler(Events::TouchEvent event);

        };

    }

}

#endif