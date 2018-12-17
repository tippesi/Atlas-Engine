#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "../System.h"

typedef struct Image {

    int32_t width;
    int32_t height;
    int32_t channels;

    vector<uint8_t> data;

};

class ImageLoader {

public:
    /**
     * Loads an image.
     * @param filename The name of the image file.
     * @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
     * @param forceChannels The number of channels to be forced. Default is zero, which means no force.
     * @return An image structure with all important data.
     */
    static Image& LoadImage(string filename, bool colorSpaceConversion, int32_t forceChannels = 0);

};



#endif
