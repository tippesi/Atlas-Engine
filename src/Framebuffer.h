#ifndef AE_FRAMEBUFFER_H
#define AE_FRAMEBUFFER_H

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Cubemap.h"
#include <unordered_map>
#include <vector>

namespace Atlas {

	namespace Texture {
		class Texture2D;
		class Texture2DArray;
		class Cubemap;
	}

	class Framebuffer {

	public:
		/**
		 * Constructs a Framebuffer object.
		 */
		Framebuffer();

		Framebuffer(const Framebuffer& that);

		/**
         * Constructs a Framebuffer object.
         * @param width
         * @param height
         */
		Framebuffer(int32_t width, int32_t height);

		/**
		 * Destructs the Framebuffer object.
		 */
		~Framebuffer();

		/**
		 *
		 * @param that
		 * @return
		 */
		//Framebuffer& operator=(Framebuffer &that);

		/**
         *
         * @param attachment
         * @param sizedFormat
         * @param wrapping
         * @param filtering
         * @param target
         */
		void AddComponent(int32_t attachment, int32_t sizedFormat, int32_t wrapping,
						  int32_t filtering, uint32_t target = GL_FRAMEBUFFER);

		/**
         *
         * @param attachment
         * @param texture
         * @param target
		 * @note The framebuffer doesn't take the ownership of the texture.
         */
		void AddComponentTexture(int32_t attachment, Texture::Texture2D *texture, uint32_t target = GL_FRAMEBUFFER);

		/**
         *
         * @param attachment
         * @param texture
         * @param layer
         * @param target
		 * @note The framebuffer doesn't take the ownership of the texture.
         */
		void AddComponentTextureArray(int32_t attachment, Texture::Texture2DArray *texture, int32_t layer,
									  uint32_t target = GL_FRAMEBUFFER);

		/**
         *
         * @param attachment
         * @param cubemap
         * @param face
         * @param target
         */
		void AddComponentCubemap(int32_t attachment, Texture::Cubemap *cubemap, int32_t face, uint32_t target = GL_FRAMEBUFFER);

		/**
         *
         * @param attachment
         * @return
         */
		Texture::Texture2D *GetComponentTexture(int32_t attachment);

		/**
         *
         * @param attachment
         * @return
         */
		Texture::Texture2DArray *GetComponentTextureArray(int32_t attachment);

		/**
         *
         * @param attachment
         * @return
         */
		Texture::Cubemap *GetComponentCubemap(int32_t attachment);

		/**
		 *
		 * @param width
		 * @param height
		 */
		void Resize(int32_t width, int32_t height);

		/**
		 *
		 * @param resizeViewport
		 */
		void Bind(bool resizeViewport = false);

		/**
		 * Unbinds any framebuffer.
		 */
		void Unbind();

		/**
		 *
		 * @param drawBuffers
		 */
		void SetDrawBuffers(std::vector<uint32_t> drawBuffers);

		int32_t width = 0;
		int32_t height = 0;

	private:
		/**
         * Represents an added component
         */
		struct Component {
			Texture::Texture2D* texture = nullptr; // If the component is a Texture2D
			Texture::Texture2DArray* textureArray = nullptr; // If the component is a Texture2DArray
			Texture::Cubemap* cubemap = nullptr; // If the component is a Cubemap
			int32_t index = 0; // The index in the cubemap (face) or the Texture2DArray layer
			uint32_t target = 0; // The target to which the component was set
			bool internalTexture = false; // Whether or not this is a texture that was created internally
		};

		Component GetComponent(int32_t attachment);

		void UpdateDrawBuffers(int32_t attachment);

		uint32_t ID;

		bool drawBuffersSet;

		std::unordered_map<int32_t, Component> components;
		std::vector<uint32_t> drawBuffers;

		static uint32_t boundFramebufferID;

	};

}

#endif
