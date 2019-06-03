#include "Cubemap.h"
#include "../loader/ImageLoader.h"
#include "../Framebuffer.h"

namespace Atlas {

   namespace Texture {

	   Cubemap::Cubemap(const Cubemap& that) {

		   DeepCopy(that);

	   }

       Cubemap::Cubemap(std::string right, std::string left, std::string top,
                        std::string bottom, std::string front, std::string back) {

           std::string filenames[] = { right, left, top, bottom, front, back };
		   Common::Image8 images[6];

		   for (int32_t i = 0; i < 6; i++) {
			   images[i] = Loader::ImageLoader::LoadImage(filenames[i], true, 3);

			   if (images[i].data.size() == 0) {
#ifdef AE_SHOW_LOG
				   AtlasLog("    Failed to load cubemap face %d %s", i, filenames[i].c_str());
#endif
				   throw AtlasException("Failed to load cubemap");
			   }

		   }

		   this->width = images[0].width;
		   this->height = images[0].height;
		   this->layers = 6;

		   Generate(GL_TEXTURE_CUBE_MAP, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR,
			   false, true);

		   for (int32_t i = 0; i < 6; i++)
			   SetData(images[i].data, i);

       }

       Cubemap::Cubemap(int32_t width, int32_t height, int32_t sizedFormat,
                        int32_t wrapping, int32_t filtering, bool generateMipmaps) {

		   this->width = width;
		   this->height = height;
		   this->layers = 6;

		   Generate(GL_TEXTURE_CUBE_MAP, sizedFormat, wrapping, filtering,
			   anisotropicFiltering, generateMipmaps);

		   glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapping);
		   glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapping);
		   glTexParameteri(target, GL_TEXTURE_WRAP_R, wrapping);

       }

       Cubemap& Cubemap::operator=(const Atlas::Texture::Cubemap &that) {

	   		if (this != &that) {

				Texture::operator=(that);

	   		}

	   		return *this;

	   }

	   void Cubemap::SetData(std::vector<uint8_t> &data, int32_t layer) {

		   Bind();
		   glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, 0, 0, 0, width, height,
			   TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

		   GenerateMipmap();

	   }

	   std::vector<uint8_t> Cubemap::GetData(int32_t layer) {

		   auto framebuffer = Framebuffer(width, height);

		   std::vector<uint8_t> data(width * height * channels * TypeFormat::GetSize(dataType));

		   framebuffer.AddComponentCubemap(GL_COLOR_ATTACHMENT0, this, layer);

		   glReadPixels(0, 0, width, height,
			   TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

		   framebuffer.Unbind();

		   return data;

	   }

	   void Cubemap::ReserveStorage(int32_t mipCount) {

		   glTexStorage2D(GL_TEXTURE_CUBE_MAP, mipCount, sizedFormat, width, height);

	   }

   }

}