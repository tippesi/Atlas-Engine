#include "Cubemap.h"
#include "../loader/ImageLoader.h"
#include "../Log.h"
#include "../volume/Frustum.h"

namespace Atlas {

   namespace Texture {

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

           format = VK_FORMAT_R16G16B16A16_SFLOAT;
           filtering = Filtering::MipMapLinear;
           wrapping = Wrapping::ClampToEdge;

           Reallocate(Graphics::ImageType::ImageCube, images[0].width, images[0].height, 6, filtering, wrapping);
           RecreateSampler(filtering, wrapping);

           for (int32_t i = 0; i < 6; i++)
               SetData(images[i].GetData(), i);

       }

       Cubemap::Cubemap(std::string filename, int32_t resolution) {

           auto image = Loader::ImageLoader::LoadImage<float>(filename, false, 4);

           if (image.GetData().size() == 0) {
               Log::Error("Failed to load cubemap image " + filename);
               return;
           }

           format = VK_FORMAT_R16G16B16A16_SFLOAT;
           filtering = Filtering::MipMapLinear;
           wrapping = Wrapping::ClampToEdge;

           Reallocate(Graphics::ImageType::ImageCube, resolution, resolution, 6, filtering, wrapping);
           RecreateSampler(filtering, wrapping);

           mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
           vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
                            vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f),
                            vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

           vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
                          vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 0.0f, 1.0f),
                          vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

           Common::Image<float> faceImage(resolution, resolution, 4);
           faceImage.fileFormat = Common::ImageFormat::HDR;

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