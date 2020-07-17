#ifndef AE_TOUCH_H
#define AE_TOUCH_H

#include "../System.h"
#include "../events/EventManager.h"
#include "../Camera.h"

namespace Atlas {

    namespace Input {

        class TouchHandler {

        public:
			TouchHandler();

			TouchHandler(const TouchHandler& that);

			TouchHandler(Camera* camera, float sensibility, float speed, float reactivity);

			~TouchHandler();

			TouchHandler& operator=(const TouchHandler& that);

			void Update(Camera* camera, float deltaTime);

			void Reset(Camera* camera);

			float sensibility = 1.5f;
			float speed = 7.0f;
			float reactivity = 6.0f;

        private:
			void RegisterEvent();

            void TouchEventHandler(Events::TouchEvent event);

			void DeepCopy(const TouchHandler& that);

			struct Finger {
				vec2 position;
				int64_t ID;
			};

			struct Finger leftFinger = { vec2(0.0f), -1 };
			struct Finger rightFinger = { vec2(0.0f), -1 };

			vec3 location = vec3(0.0f);
			vec2 rotation = vec2(0.0f);

			int32_t eventHandle;

        };

    }

}

#endif