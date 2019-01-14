#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include "../System.h"

#include <vector>

#define IMAGE_PNG 0
#define IMAGE_JPG 1
#define IMAGE_BMP 2
#define IMAGE_PGM 3

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
     * Save an image with 8 bits per channel to the hard drive.
     * @param image The image to be stored
     * @param filename The filename that the image should have
     * @note By changing the fileFormat in the Image object the output file changes as well.
     */
    static void SaveImage(Image& image, string filename);

	/**
     * Save an image with 16 bits per channel to the hard drive.
     * @param image The image to be stored
     * @param filename The filename that the image should have
     * @note By changing the fileFormat in the Image16 object the output file changes as well. Only
     * IMAGE_PGM is supported for now.
     */
	static void SaveImage16(Image16& image, string filename);

private:
	static void SavePGM8(Image& image, string filename);

	static void SavePGM16(Image16& image, string filename);

};

#endif