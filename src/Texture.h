#ifndef TEXTURE_H
#define TEXTURE_H

#include "System.h"

class Texture {

public:
	/// <summary>
	/// Constructs a <see cref="Texture"/>.
	/// </summary>
	Texture(GLenum dataFormat, int32_t width, int32_t height, int32_t internalFormat,
		float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps);

	/// <summary>
	/// Constructs a <see cref="Texture"/>.
	/// </summary>
	/// <param name="filename">The name of the image which should be loaded.</param>
	/// <param name="withoutCorrection">Removes gamma correction if set to true (useful for normal maps or height maps)</param>
	Texture(const char* filename, bool withoutCorrection = false);

	/// <summary>
	/// Binds the texture to a certain OpenGL texture unit
	/// </summary>
	/// <param name="unit">The unit where the texture should be bound.</param>
	void Bind(uint32_t unit);

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
	/// Resizes the texture. Results in a loss of data.
	/// </summary>
	/// <param name="width">The new width of the texture.</param>
	/// <param name="height">The new height of the texture.</param>
	void Resize(int32_t width, int32_t height);

	/// <summary>
	/// Flips the data horizontally. OpenGL stores data upside down.
	/// </summary>
	void FlipHorizontally();

	/// <summary>
	/// Saves the texture as a portable network graphics.
	/// </summary>
	/// <param name="filename">The name of the file where the texture should be stored.</param>
	void SaveToPNG(const char* filename);

	/// <summary>
	/// Gets the OpenGL texture ID.
	/// </summary>
	uint32_t GetID();

	~Texture();

	int32_t width;
	int32_t height;

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

	/// <summary>
	/// Removes gamma correction of image data (sRGB to RGB)
	/// </summary>
	static void UncorrectGamma(uint8_t* data, int32_t width, int32_t height, int32_t channels, float gamma);

private:
	/// <summary>
	/// Flips texture data.
	/// </summary>
	/// <param name="data">The data which should be used. Each channel should have 8 bits and should be in the following order (r, g, b, a).</param>
	/// <return>The data where each channel as 8 bits in the following order (r, g, b, a).</return>
	uint8_t* FlipDataHorizontally(uint8_t* data);

	/// <summary>
	/// Generates an OpenGL texture and uploads the data to the GPU
	/// </summary>
	void GenerateTexture(GLenum dataFormat, int32_t internalFormat,
		int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps);

	/// OpenGL texture ID
	uint32_t ID;

	uint8_t* data;

	GLenum dataFormat;
	int32_t internalFormat;

	bool mipmaps;

	/// Static members
	/// The current anisotropicLevel
	static int32_t anisotropyLevel;
	/// Returns the base format of a sized internal format
	static int32_t GetBaseFormat(int32_t sizedFormat);

};

#endif