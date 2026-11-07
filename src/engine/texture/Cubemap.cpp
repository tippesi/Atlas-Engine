#include "Cubemap.h"
#include "../loader/ImageLoader.h"
#include "../Log.h"

namespace Atlas {

   namespace Texture {

       Cubemap::Cubemap(const std::string& right, const std::string& left, const std::string& top,
           const std::string& bottom, const std::string& front, const std::string& back) {

           std::string filenames[] = { right, left, top, bottom, front, back };
           Ref<Common::Image<uint8_t>> images[6];

           for (int32_t i = 0; i < 6; i++) {
               images[i] = Loader::ImageLoader::LoadImage<uint8_t>(filenames[i], true, 4);

               if (images[i]->GetData().size() == 0) {
                   Log::Error("    Failed to load cubemap face " + std::to_string(i) + " " + filenames[i]);
                   return;
               }

           }

           format = VK_FORMAT_R16G16B16A16_SFLOAT;
           filtering = Filtering::MipMapLinear;
           wrapping = Wrapping::ClampToEdge;

           Reallocate(Graphics::ImageType::ImageCube, images[0]->width, images[0]->height, 6, filtering, wrapping);
           RecreateSampler(filtering, wrapping);

           for (int32_t i = 0; i < 6; i++)
               SetData(images[i]->GetData(), i);

       }

       Cubemap::Cubemap(const std::string& filename) {

           auto image = Loader::ImageLoader::LoadImage<float>(filename, false, 4);

           if (image->GetData().size() == 0) {
               Log::Error("Failed to load cubemap image " + filename);
               return;
           }

           format = VK_FORMAT_R16G16B16A16_SFLOAT;
           filtering = Filtering::MipMapLinear;
           wrapping = Wrapping::ClampToEdge;

           auto resolution = image->width / 4;
           Reallocate(Graphics::ImageType::ImageCube, resolution, resolution, 6, filtering, wrapping);
           RecreateSampler(filtering, wrapping);

           // Direction of a texel according to the cube map face selection of Vulkan
           auto faceDirection = [](uint8_t face, vec2 coord) {
               switch (face) {
               case 0: return vec3(1.0f, -coord.y, -coord.x);
               case 1: return vec3(-1.0f, -coord.y, coord.x);
               case 2: return vec3(coord.x, 1.0f, coord.y);
               case 3: return vec3(coord.x, -1.0f, -coord.y);
               case 4: return vec3(coord.x, -coord.y, 1.0f);
               default: return vec3(-coord.x, -coord.y, -1.0f);
               }
           };

           Common::Image<float> faceImage(resolution, resolution, 4);
           faceImage.fileFormat = Common::ImageFormat::HDR;

           for (uint8_t i = 0; i < 6; i++) {
               for (int32_t y = 0; y < resolution; y++) {
                   for (int32_t x = 0; x < resolution; x++) {
                       auto coord = glm::vec2(((float)x + 0.5f) / (float)resolution,
                           ((float)y + 0.5f) / (float)resolution);

                       auto direction = glm::normalize(faceDirection(i, coord * 2.0f - 1.0f));

                       // Cubemap direction to equirectangular image coordinates, the first image row is the zenith
                       const vec2 invAtan = vec2(0.1591f, 0.3183f);
                       vec2 uv = vec2(glm::atan(direction.z, direction.x), -asin(direction.y));
                       uv *= invAtan;
                       uv += 0.5f;

                       vec3 sample = glm::clamp(vec3(image->SampleBilinear(uv.x, uv.y)),
                           vec3(0.0f), vec3(65500.0f));

                       faceImage.SetData(x, y, 0, sample.x);
                       faceImage.SetData(x, y, 1, sample.y);
                       faceImage.SetData(x, y, 2, sample.z);
                       faceImage.SetData(x, y, 3, 0.0f);

                   }
               }

               std::vector<float16> halfFloatData;
               for (auto data : faceImage.GetData()) {
                   halfFloatData.push_back(glm::detail::toFloat16(data));
               }

               SetData(halfFloatData, i);
        
           }

       }

       Cubemap::Cubemap(int32_t width, int32_t height, VkFormat format,
           Wrapping wrapping, Filtering filtering) {

           this->format = format;
           Reallocate(Graphics::ImageType::ImageCube, width, height, 6, filtering, wrapping);
           RecreateSampler(filtering, wrapping);

       }

       void Cubemap::SetData(std::vector<uint8_t> &data, int32_t layer) {

           Texture::SetData(data.data(), 0, 0, layer, width, height, 1);

           GenerateMipmap();

       }

       void Cubemap::SetData(std::vector<float>& data, int32_t layer) {

           Texture::SetData(data.data(), 0, 0, layer, width, height, 1);

           GenerateMipmap();

       }

       void Cubemap::SetData(std::vector<float16>& data, int32_t layer) {

           Texture::SetData(data.data(), 0, 0, layer, width, height, 1);

           GenerateMipmap();

       }

       std::vector<uint8_t> Cubemap::GetData(int32_t layer) {

           // auto framebuffer = Framebuffer(width, height);

           // std::vector<uint8_t> data(width * height * channels * TypeFormat::GetSize(dataType));

           // framebuffer.AddComponentCubemap(GL_COLOR_ATTACHMENT0, this, layer);

           // glReadPixels(0, 0, width, height,
           //    TextureFormat::GetBaseFormat(sizedFormat), dataType, data.data());

           // framebuffer.Unbind();

           return std::vector<uint8_t>();

       }

   }

}