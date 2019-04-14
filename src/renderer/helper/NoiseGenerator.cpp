#include "NoiseGenerator.h"

#include <vector>

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			void NoiseGenerator::GenerateNoiseTexture2D(Texture::Texture2D& texture) {

				auto formatSize = TypeFormat::GetSize(texture.GetDataType());

				std::vector<uint8_t> data(formatSize * texture.channels * texture.width * texture.height);

				for (size_t i = 0; i < data.size(); i++) {
					data[i] = rand() % 255;
				}

				texture.SetData(data);

			}

		}

	}

}