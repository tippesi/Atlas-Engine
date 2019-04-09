#ifndef AE_VIEWPORT_H
#define AE_VIEWPORT_H

#include "System.h"
#include "Camera.h"

namespace Atlas {

	/**
	 * Represents a rectangular area inside a window where the engine renders to.
	 */
	class Viewport {

	public:
		/**
		 * Constructs a Viewport object.
		 */
		Viewport() {}

		/**
         * Constructs a Viewport object.
         * @param x The offset in x direction to the upper left corner of the window
         * @param y The offset in y direction to the upper left corner of the window
         * @param width The width in pixels of the viewport
         * @param height The height in pixels of the viewport
         */
		Viewport(int32_t x, int32_t y, int32_t width, int32_t height);

		/**
         * Resets the size of the viewport.
          * @param x The offset in x direction to the upper left corner of the window
         * @param y The offset in y direction to the upper left corner of the window
         * @param width The width in pixels of the viewport
         * @param height The height in pixels of the viewport
         */
		void Set(int32_t x, int32_t y, int32_t width, int32_t height);

		/**
         * Transforms a point from screen space pixel coordinates into world space.
         * @param point The point to be transformed.
         * @param camera A camera which is used for the transformation
         * @return The transformed point in world space.
         * @remark The x and y components of the point are just a position in pixels relative to the window.
         * The z component represents the depth of the later transformed point and can be in range (0-1).
         * If z is equal to zero the point will be in the exact depth as the near plane of the camera.
         * If z is equal to one the point will be in the exact depth as the far plane of the camera.
         */
		vec3 Unproject(vec3 point, Camera *camera);

		int32_t x = 0;
		int32_t y = 0;

		int32_t width = 0;
		int32_t height = 0;

	};

}

#endif