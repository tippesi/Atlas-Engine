#ifndef AE_TOUCH_H
#define AE_TOUCH_H

#include "../System.h"
#include "../events/EventManager.h"
#include "../Camera.h"

namespace Atlas {

    namespace Input {

        class TouchHandler {

        public:
			TouchHandler(Camera* camera, float sensibility, float speed, float reactivity);

			void Update(Camera* camera, float deltaTime);

			float sensibility;
			float speed;
			float reactivity;
			float threshold;

        private:
            void TouchEventHandler(Events::TouchEvent event);

			struct Finger {
				vec2 position;
				int64_t ID;
			};

			struct Finger leftFinger;
			struct Finger rightFinger;

			vec3 location;
			vec2 rotation;

        };

    }

}

#endif