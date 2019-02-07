#ifndef AE_CUBEMAP_H
#define AE_CUBEMAP_H

#include "System.h"

namespace Atlas {

	namespace Texture {

		class Cubemap {

		public:
			/**
             *
             * @param right
             * @param left
             * @param top
             * @param bottom
             * @param front
             * @param back
             */
			Cubemap(std::string right, std::string left, std::string top,
					std::string bottom, std::string front, std::string back);

			/**
             *
             * @param dataFormant
             * @param width
             * @param height
             * @param internalFormat
             * @param wrapping
             * @param filtering
             * @param mipmaps
             */
			Cubemap(GLenum dataFormant, int32_t width, int32_t height, int32_t internalFormat,
					int32_t wrapping, int32_t filtering, bool mipmaps);

			~Cubemap();

			/**
             *
             * @param unit
             */
			void Bind(uint32_t unit);

			/**
             *
             * @return
             */
			uint32_t GetID();

		private:
			uint32_t ID;

		};

	}

}

#endif