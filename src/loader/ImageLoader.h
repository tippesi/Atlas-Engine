#ifndef AE_IMAGELOADER_H
#define AE_IMAGELOADER_H

#include "../System.h"
#include "../common/Image.h"

#include <vector>



namespace Atlas {

	namespace Loader {

        class ImageLoader {

        public:
            /**
             * Loads an image with 8 bits per channel.
             * @param filename The name of the image file.
             * @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
             * @param forceChannels The number of channels to be forced. Default is zero, which means no force.
             * @return An Image object with all the important data.
             */
            static Common::Image8 LoadImage(std::string filename, bool colorSpaceConversion, int32_t forceChannels = 0);

            /**
            * Loads an image with 16 bits per channel.
            * @param filename The name of the image file.
            * @param colorSpaceConversion Whether or not gamma to linear color space conversion is needed.
            * @param forceChannels The number of channels to be forced. Default is zero, which means no force.
            * @return An Image16 object with all the important data.
            */
            static Common::Image16 LoadImage16(std::string filename, bool colorSpaceConversion, int32_t forceChannels = 0);

            /**
             * Save an image with 8 bits per channel to the hard drive.
             * @param image The image to be stored
             * @param filename The filename that the image should have
             * @note By changing the fileFormat in the Image object the output file changes as well.
             */
            static void SaveImage(Common::Image8& image, std::string filename);

            /**
             * Save an image with 16 bits per channel to the hard drive.
             * @param image The image to be stored
             * @param filename The filename that the image should have
             * @note By changing the fileFormat in the Image16 object the output file changes as well. Only
             * IMAGE_PGM is supported for now.
             */
            static void SaveImage16(Common::Image16& image, std::string filename);

        private:
            static void SavePGM8(Common::Image8& image, std::string filename);

            static void SavePGM16(Common::Image16& image, std::string filename);

        };

    }

}

#endif