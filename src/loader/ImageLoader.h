#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "../System.h"

#include <vector>

#define IMAGE_PNG 0
#define IMAGE_JPG 1
#define IMAGE_BMP 2

/**
 * Base class for the image classes.
 */
class IImage {

public:
	int32_t width;
	int32_t height;
	int32_t channels;

	int32_t fileFormat;

};

/**
 * Represents an image with 8 bits per channel.
 */
class Image : public IImage {

public:
    vector<uint8_t> data;

};

/**
* Represents an image with 16 bits per channel.
*/
class Image16 : public IImage {

public:
	vector<uint16_t> data;

};

class ImageLoader {

public:
    /**
     * Loads an image with 8 bits per channel.
     * @param filename The name of the image file.
     * @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
     * @param forceChannels The number of channels to be forced. Default is zero, which means no force.
     * @return An Image object with all the important data.
     */
    static Image LoadImage(string filename, bool colorSpaceConversion, int32_t forceChannels = 0);

	/**
	* Loads an image with 16 bits per channel.
	* @param filename The name of the image file.
	* @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
	* @param forceChannels The number of channels to be forced. Default is zero, which means no force.
	* @return An Image16 object with all the important data.
	*/
	static Image16 LoadImage16(string filename, bool colorSpaceConversion, int32_t forceChannels = 0);

    /**
     *
     * @param filename
     * @param image
     */
    static void SaveImage(string filename, Image& image);

};



#endif
