#include "ImageLoader.h"
#include "../texture/Texture.h"

#include <algorithm>

//STB image library is declared(only once)
#define STB_IMAGE_IMPLEMENTATION
#include "libraries/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libraries/stb/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "libraries/stb/stb_image_resize.h"

Image ImageLoader::LoadImage(string filename, bool colorSpaceConversion, int32_t forceChannels) {

    Image image;

    auto data = stbi_load(filename.c_str(), &image.width, &image.height, &image.channels, forceChannels);

    if (data == nullptr) {
#ifdef ENGINE_SHOW_LOG
        EngineLog("Failed to load image %s", filename.c_str());
#endif
        throw EngineException("Image couldn't be loaded");
    }

    image.data.assign(data, data + image.width * image.height * image.channels);
    delete[] data;

    if (forceChannels > 0) {
        image.channels = forceChannels;
    }

    if (colorSpaceConversion) {
        Texture::GammaToLinear(image.data, image.width, image.height, image.channels);
    }

    auto fileFormatPosition = filename.find_last_of('.') + 1;
    auto fileFormat = filename.substr(fileFormatPosition, filename.length());

    std::transform(fileFormat.begin(), fileFormat.end(), fileFormat.begin(), ::tolower);

    if (fileFormat == "png") {
        image.fileFormat = IMAGE_PNG;
    }
    else if (fileFormat == "jpg" || fileFormat == "jpeg") {
        image.fileFormat = IMAGE_JPG;
    }
    else if (fileFormat == "bmp") {
        image.fileFormat = IMAGE_BMP;
    }

#ifdef ENGINE_SHOW_LOG
    EngineLog("Loaded image %s", filename.c_str());
#endif

    return image;

}

void ImageLoader::SaveImage(string filename, Image &image) {

    switch(image.fileFormat) {
        case IMAGE_JPG: break;
        case IMAGE_BMP: break;
        default: break;
    }

}