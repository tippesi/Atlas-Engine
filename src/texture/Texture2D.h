#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#include "../System.h"
#include "Texture.h"

class Texture2D : public Texture {

public:
    /**
     * Constructs a Texture2D object.
     * @param dataFormat
     * @param width
     * @param height
     * @param internalFormat
     * @param wrapping
     * @param filtering
     * @param anisotropicFiltering
     * @param generateMipMaps
     */
    Texture2D(GLenum dataType, int32_t width, int32_t height, int32_t sizedFormat,
              int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

    /**
     * Constructs a Texture2D object from an image file.
     * @param filename
     * @param colorSpaceConversion
     * @param anisotropicFiltering
     * @param generateMipMaps
     */
    Texture2D(string filename, bool colorSpaceConversion = true, bool anisotropicFiltering = true,
            bool generateMipMaps = true);

    /**
     * Binds the texture to a texture unit
     * @param unit The texture unit the texture should be bound to.
     * @note The texture unit should be between GL_TEXTURE0-GL_TEXTURE_MAX
     */
    void Bind(uint32_t unit);

    /**
     * Unbinds any texture.
     */
    void Unbind();

    /**
     * Sets the data of the texture
     * @param data A vector holding the new data.
     */
    void SetData(vector<uint8_t>& data);

    /**
     * Retrieves the data of the texture from the GPU.
     * @return A vector holding the data.
     */
    vector<uint8_t> GetData();

    /**
     * Resizes the texture
     * @param width The new width of the texture.
     * @param height The new height of the texture.
     * @warning This results in a loss of texture data.
     */
    void Resize(int32_t width, int32_t height);

    /**
     * Saves the texture to a PNG image file
     * @param filename The name of the file
     */
    void SaveToPNG(string filename);

protected:
    void ReserveStorage(int32_t mipCount);

};

#endif