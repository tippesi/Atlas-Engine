#ifndef TEXTURE_H
#define TEXTURE_H

#include "system.h"

class Texture {

public:
	/// <summary>
	/// Constructs a <see cref="Texture"/>.
	/// </summary>
	Texture(void* data, GLenum dataFormat, int32_t width, int32_t height, int32_t internalFormat,
		int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps);

	/// <summary>
	/// Constructs a <see cref="Texture"/>.
	/// </summary>
	/// <param name="filename">The name of the image which should be loaded.</param>
	/// <param name="withoutCorrection">Removes gamma correction if set to true (useful for normal maps or height maps)</param>
	Texture(const char* filename, bool withoutCorrection = false);

	/// <summary>
	/// Sets the data of the texture.
	/// </summary>
	/// <param name="data">The data which should be used. Each channel should have 8 bits and should be in the following order (r, g, b, a).</param>
	void SetData(uint8_t* data);

	/// <summary>
	/// Gets a copy of the data of the texture. Retrieval of the GPU data is only available for OpenGL NOT OpenGL ES
	/// </summary>
	/// <return>The data where each channel as 8 bits in the following order (r, g, b, a).</return>
	uint8_t* GetData();

	/// <summary>
	/// Mirrors the data horizontally. OpenGL stores data upside down.
	/// </summary>
	void MirrorHorizontally();

	/// <summary>
	/// Saves the texture to a file.
	/// </summary>
	/// <param name="filename">The name of the file where the texture should be stored.</param>
	void SaveToFile(const char* filename);

	~Texture();

	int32_t width;
	int32_t height;

	int32_t format;
	int32_t channels;

	/// Static members
	/// <summary>
	/// Determines the maximum anisotropy level
	/// </summary>
	/// <returns>Returns the maximum anisotropy level. If anisotropic filtering isn't available the
	/// the return value will be zero.</returns>
	static int32_t GetMaxAnisotropyLevel();

	/// <summary>
	/// Sets the level of anisotropy if available
	/// </summary>
	/// <param name="anisotropyLevel">The level of anisotropy applied to the textures.</param>
	static void SetAnisotropyLevel(int32_t anisotropyLevel);

private:
	/// <summary>
	/// Removes gamma correction of image data (sRGB to RGB)
	/// </summary>
	void UncorrectGamma();

	/// <summary>
	/// Mirrors texture data.
	/// </summary>
	/// <param name="data">The data which should be used. Each channel should have 8 bits and should be in the following order (r, g, b, a).</param>
	/// <return>The data where each channel as 8 bits in the following order (r, g, b, a).</return>
	uint8_t* MirrorDataHorizontally(uint8_t* data);

	/// <summary>
	/// Generates an OpenGL texture and uploads the data to the GPU
	/// </summary>
	void GenerateTexture(GLenum dataFormat, int32_t internalFormat,
		int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps);

	/// OpenGL texture ID
	uint32_t ID;

	uint8_t* data;

	/// Static members
	/// The current anisotropicLevel
	static int32_t anisotropyLevel;

};

#endif