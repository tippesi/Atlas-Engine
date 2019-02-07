#ifndef AE_RENDERTARGET_H
#define AE_RENDERTARGET_H

#include "System.h"
#include "Framebuffer.h"

namespace Atlas {

	class RenderTarget {

	public:
		///
		/// \param width
		/// \param height
		RenderTarget(int32_t width, int32_t height);

		///
		/// \param width
		/// \param height
		void Resize(int32_t width, int32_t height);

		~RenderTarget();

		Framebuffer *geometryFramebuffer;
		Framebuffer *lightingFramebuffer;

		int32_t width;
		int32_t height;

	};

}


#endif