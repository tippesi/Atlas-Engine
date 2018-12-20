#ifndef TEXTURE2DARRAY_H
#define TEXTURE2DARRAY_H

#include "../System.h"
#include "Texture.h"

class Texture2DArray : public Texture {

public:
    /**
     * Construct a Texture2DArray object.
     * @param dataType
     * @param width
     * @param height
     * @param depth
     * @param sizedFormat
     * @param wrapping
     * @param filtering
     * @param anisotropicFiltering
     * @param generateMipMaps
     */
    Texture2DArray(GLenum dataType, int32_t width, int32_t height, int32_t depth, int32_t sizedFormat,
            int32_t wrapping, int32_t filtering, bool anisotropicFiltering, bool generateMipMaps);

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
     * @param depth The depth where the data should be set.
     * @param count The number of layer where the data should be written to.
     * @note The data has to have the size of count * width * height * channels.
     */
    void SetData(vector<uint8_t>& data, int32_t depth, int32_t count = 1);

    /**
     * Retrieves the data of the texture from the GPU.
     * @param depth The depth where the data should be retrieved.
     * @return A vector holding the data.
     */
    vector<uint8_t> GetData(int32_t depth);

    /**
     * Resizes the texture
     * @param width The new width of the texture.
     * @param height The new height of the texture.
     * @param depth The new depth of the texture.
     * @warning This results in a loss of texture data.
     */
    void Resize(int32_t width, int32_t height, int32_t depth);

    /**
     * Saves a layer of the texture to a PNG image file
     * @param filename The name of the file
     * @param depth This results in a loss of texture data and a
     * change of the texture id.
     */
    void SaveToPNG(string filename, int32_t depth);

protected:
    void ReserveStorage(int32_t mipCount);

};

#endif