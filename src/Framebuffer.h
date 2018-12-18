#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "Cubemap.h"
#include <unordered_map>
#include <vector>

class Framebuffer {

public:
	/**
	 *
	 * @param width
	 * @param height
	 */
	Framebuffer(int32_t width, int32_t height);

	/**
	 *
	 * @param attachment
	 * @param dataFormat
	 * @param internalFormat
	 * @param wrapping
	 * @param filtering
	 */
	void AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping,
			int32_t filtering, uint32_t target = GL_FRAMEBUFFER);

	/**
	 *
	 * @param attachment
	 * @param texture
	 */
	void AddComponentTexture(int32_t attachment, Texture2D* texture, uint32_t target = GL_FRAMEBUFFER);

	/**
	 *
	 * @param attachment
	 * @param texture
	 * @param layer
	 */
	void AddComponentTextureArray(int32_t attachment, Texture2DArray* texture, int32_t layer,
			uint32_t target = GL_FRAMEBUFFER);

	/**
	 *
	 * @param attachment
	 * @param cubemap
	 * @param face
	 */
	void AddComponentCubemap(int32_t attachment, Cubemap* cubemap, int32_t face, uint32_t target = GL_FRAMEBUFFER);

	/**
	 *
	 * @param attachment
	 * @return
	 */
	Texture2D* GetComponentTexture(int32_t attachment);

	/**
	 *
	 * @param attachment
	 * @return
	 */
	Texture2DArray* GetComponentTextureArray(int32_t attachment);

	/**
	 *
	 * @param attachment
	 * @return
	 */
	Cubemap* GetComponentCubemap(int32_t attachment);

	///
	/// \param width
	/// \param height
	void Resize(int32_t width, int32_t height);

	///
	/// \param resizeViewport
	void Bind(bool resizeViewport = false);

	///
	void Unbind();

	///
	/// \param buffers
	/// \param count
	void SetDrawBuffers(uint32_t* buffers, int32_t count);

	///
	void ClearContent();

	///
	void DeleteContent();

	~Framebuffer();

	int32_t width;
	int32_t height;

private:
	struct FramebufferComponent {
		Texture2D* texture;
		Texture2DArray* textureArray;
		Cubemap* cubemap;
		bool internalTexture;
	};

	uint32_t ID;

	bool drawBuffersSet;

	unordered_map<int32_t, FramebufferComponent*> components;
	vector<uint32_t> drawBuffers;

	static uint32_t boundFramebufferID;

};

#endif