#include "Cubemap.h"
#include "texture/Texture.h"
#include "loader/ImageLoader.h"

namespace Atlas {

   namespace Texture {

       Cubemap::Cubemap(std::string right, std::string left, std::string top,
                        std::string bottom, std::string front, std::string back) {

           std::string filenames[] = { right, left, top, bottom, front, back };

           glGenTextures(1, &ID);

           glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

#ifdef AE_SHOW_LOG
           AtlasLog("Loading cubemap with ID %d", ID);
#endif

           for (uint32_t i = 0; i < 6; i++) {

               auto image = Loader::ImageLoader::LoadImage(filenames[i], false, 3);

               if (image.data.size() != 0) {
#ifdef AE_API_GL
                   glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, AE_SRGB8, image.width, image.height, 0,
                                AE_RGB, AE_UBYTE, image.data.data());
#elif AE_API_GLES
                   Texture::GammaToLinear(image.data.data(), image.width, image.height, 3);

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, AE_RGB8, image.width, image.height, 0,
				AE_RGB, AE_UBYTE, image.data.data());
#endif
#ifdef AE_SHOW_LOG
                   AtlasLog("    Loaded cubemap face %d %s", i, filenames[i].c_str());
#endif
               }
               else {
#ifdef AE_SHOW_LOG
                   AtlasLog("    Failed to load cubemap face %d %s", i, filenames[i].c_str());
#endif
                   throw AtlasException("Failed to load cubemap");
               }

           }

           glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

           glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

       }

       Cubemap::Cubemap(GLenum dataFormant, int32_t width, int32_t height, int32_t internalFormat,
                        int32_t wrapping, int32_t filtering, bool mipmaps) {

           glGenTextures(1, &ID);

           glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

           if (mipmaps) {
               glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
               glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
           }
           else {
               glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filtering);
               glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filtering);
           }

           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrapping);
           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrapping);
           glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrapping);

           for (uint32_t i = 0; i < 6; i++) {

               glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0,
                            TextureFormat::GetBaseFormat(internalFormat), dataFormant, NULL);

           }

           if (mipmaps) {
               glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
           }

           glBindTexture(GL_TEXTURE_CUBE_MAP, 0);


       }

       Cubemap::~Cubemap() {

           glDeleteTextures(1, &ID);

       }

       void Cubemap::Bind(uint32_t unit) {

           glActiveTexture(unit);
           glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

       }

       uint32_t Cubemap::GetID() {

           return ID;

       }

   }

}