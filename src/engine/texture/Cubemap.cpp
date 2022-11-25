#include "Cubemap.h"
#include "../loader/ImageLoader.h"
#include "../Framebuffer.h"
#include "../Log.h"
#include "../volume/Frustum.h"

namespace Atlas {

   namespace Texture {

	   Cubemap::Cubemap(const Cubemap& that) {

		   DeepCopy(that);

	   }

       Cubemap::Cubemap(std::string right, std::string left, std::string top,
                        std::string bottom, std::string front, std::string back) {

           std::string filenames[] = { right, left, top, bottom, front, back };
		   Common::Image<uint8_t> images[6];

		   for (int32_t i = 0; i < 6; i++) {
			   images[i] = Loader::ImageLoader::LoadImage<uint8_t>(filenames[i], true, 4);

			   if (images[i].GetData().size() == 0) {
				   Log::Error("    Failed to load cubemap face " + std::to_string(i) + " " + filenames[i]);
				   return;
			   }

		   }

		   this->width = images[0].width;
		   this->height = images[0].height;
		   this->layers = 6;

		   Generate(GL_TEXTURE_CUBE_MAP, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR,
			   false, true);

		   for (int32_t i = 0; i < 6; i++)
			   SetData(images[i].GetData(), i);

       }

	   Cubemap::Cubemap(std::string filename, int32_t resolution) {

		   auto image = Loader::ImageLoader::LoadImage<float>(filename, false, 3);

		   if (image.GetData().size() == 0) {
			   Log::Error("Failed to load cubemap image " + filename);
			   return;
		   }

		   this->width = resolution;
		   this->height = resolution;
		   this->layers = 6;

		   Generate(GL_TEXTURE_CUBE_MAP, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR,
			   false, true);

		   mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
		   vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
							vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f),
							vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

		   vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
						  vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 0.0f, 1.0f),
						  vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

		   Common::Image<float> faceImage(resolution, resolution, 4);
		   faceImage.fileFormat = AE_IMAGE_HDR;

		   for (uint8_t i = 0; i < 6; i++) {
			   Volume::Frustum frustum(projectionMatrix *
				   glm::lookAt(vec3(0.0f), faces[i], ups[i]));
			   auto corners = frustum.GetCorners();

			   vec3 planeOrigin = corners[0];
			   vec3 planeRight = corners[1] - corners[0];
			   vec3 planeBottom = corners[2] - corners[0];

			   for (int32_t y = 0; y < resolution; y++) {
				   for (int32_t x = 0; x < resolution; x++) {
					   auto coord = glm::vec2(((float)x + 0.5f) / (float)resolution,
						   ((float)y + 0.5f) / (float)resolution);

					   auto direction = glm::normalize(planeOrigin + coord.x * planeRight
						   + coord.y * planeBottom);

					   // Cubemap direction to equirectangular image coordinates
					   const vec2 invAtan = vec2(0.1591f, 0.3183f);
					   vec2 uv = vec2(glm::atan(direction.z, direction.x), asin(direction.y));
					   uv *= invAtan;
					   uv += 0.5f;

					   vec3 sample = glm::clamp(vec3(image.SampleBilinear(uv.x, uv.y)), 
						   vec3(0.0f), vec3(65500.0f));

					   faceImage.SetData(x, y, 0, sample.x);
					   faceImage.SetData(x, y, 1, sample.y);
					   faceImage.SetData(x, y, 2, sample.z);
					   faceImage.SetData(x, y, 3, 0.0f);

				   }
			   }

			   SetData(faceImage.GetData(), i);
		
		   }

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
			   TextureFormat::GetBaseFormat(sizedFormat), AE_UBYTE, data.data());

		   GenerateMipmap();

	   }

	   void Cubemap::SetData(std::vector<float>& data, int32_t layer) {

		   Bind();
		   glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, 0, 0, 0, width, height,
			   TextureFormat::GetBaseFormat(sizedFormat), AE_FLOAT, data.data());

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