#include "MaterialLoader.h"
#include "AssetLoader.h"
#include "ImageLoader.h"
#include "../common/Path.h"
#include "../Log.h"

#include <stb_image.h>
#include <stb_image_write.h>

namespace Atlas {

    namespace Loader {
		Ref<Material> MaterialLoader::LoadMaterial(std::string filename, int32_t mapResolution) {

			auto stream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

			if (!stream.is_open()) {
				Log::Error("Failed to load material " + filename);
				return nullptr;
			}

			int32_t textureCount = 0;
			auto material = LoadMaterialValues(stream, textureCount);

			if (!material) {
				Log::Error("File isn't a material file " + filename);
				return nullptr;
			}

			auto materialDirectory = Common::Path::GetDirectory(filename);

			std::string line;
			for (int32_t i = 0; i < textureCount; i++) {
				std::getline(stream, line);
				auto prefix = line.substr(0, 3);
				if (prefix == "BMP") {
					material->baseColorMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->baseColorMapPath, true, 4);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->baseColorMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->baseColorMap->SetData(image.GetData());
				}
				if (prefix == "OMP") {
					material->opacityMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->opacityMapPath, false, 1);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->baseColorMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->baseColorMap->SetData(image.GetData());
				}
				else if (prefix == "NMP") {
					material->normalMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->normalMapPath, false, 4);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->normalMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->normalMap->SetData(image.GetData());
				}
				else if (prefix == "RMP") {
					material->roughnessMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->roughnessMapPath, false, 1);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->roughnessMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->roughnessMap->SetData(image.GetData());
				}
				else if (prefix == "MMP") {
					material->metalnessMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->metalnessMapPath, false, 1);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->metalnessMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->metalnessMap->SetData(image.GetData());
				}
				else if (prefix == "AMP") {
					material->aoMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->aoMapPath, false, 1);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->aoMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->aoMap->SetData(image.GetData());
				}
				else if (prefix == "DMP") {
					material->displacementMapPath = ReadFilePath(line, materialDirectory);
					auto image = ImageLoader::LoadImage<uint8_t>(material->displacementMapPath, false, 1);
					if ((image.width != mapResolution || image.height != mapResolution) && mapResolution)
						image.Resize(mapResolution, mapResolution);
                    material->displacementMap = std::make_shared<Texture::Texture2D>(image.width, image.height,
                        VK_FORMAT_R8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear);
					material->displacementMap->SetData(image.GetData());
				}
			}

			stream.close();

			return material;

		}

        void MaterialLoader::SaveMaterial(Ref<Material> material, std::string filename) {

            auto stream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!stream.is_open()) {
                Log::Error("Failed to save material " + filename);
				return;
            }

			std::string header, body;
			int32_t textureCount = 0;

			header.append("AEM ");

			textureCount += material->HasBaseColorMap() ? 1 : 0;
			textureCount += material->HasNormalMap() ? 1 : 0;
			textureCount += material->HasRoughnessMap() ? 1 : 0;
			textureCount += material->HasMetalnessMap() ? 1 : 0;
			textureCount += material->HasAoMap() ? 1 : 0;
			textureCount += material->HasDisplacementMap() ? 1 : 0;

			header.append(std::to_string(textureCount) + "\n");

			stream << header;

			body.append("N " + material->name + "\n");

            body.append(WriteVector("BC", material->baseColor));
			body.append(WriteVector("EC", material->emissiveColor));
			body.append(WriteVector("TC", material->transmissiveColor));

            body.append("RN " + std::to_string(material->roughness) + "\n");
            body.append("MN " + std::to_string(material->metalness) + "\n");
            body.append("AO " + std::to_string(material->ao) + "\n");
            body.append("RF " + std::to_string(material->reflectance) + "\n");
			body.append("NS " + std::to_string(material->normalScale) + "\n");
            body.append("DS " + std::to_string(material->displacementScale) + "\n");
            body.append("TL " + std::to_string(material->tiling) + "\n");

            stream << body;
			stream << "\n";

			auto materialPath = AssetLoader::GetFullPath(filename);

			if (material->HasBaseColorMap())
				stream << "BMP " + Common::Path::GetRelative(materialPath, material->baseColorMapPath) + "\n";
			if (material->HasOpacityMap())
				stream << "OMP " + Common::Path::GetRelative(materialPath, material->opacityMapPath) + "\n";
			if (material->HasNormalMap())
				stream << "NMP " + Common::Path::GetRelative(materialPath, material->normalMapPath) + "\n";
			if (material->HasRoughnessMap())
				stream << "RMP " + Common::Path::GetRelative(materialPath, material->roughnessMapPath) + "\n";
			if (material->HasMetalnessMap())
				stream << "MMP " + Common::Path::GetRelative(materialPath, material->metalnessMapPath) + "\n";
			if (material->HasAoMap())
				stream << "AMP " + Common::Path::GetRelative(materialPath, material->aoMapPath) + "\n";
			if (material->HasDisplacementMap())
				stream << "DMP " + Common::Path::GetRelative(materialPath, material->displacementMapPath) + "\n";

            stream.close();

        }

		Ref<Material> MaterialLoader::LoadMaterialValues(std::ifstream& stream, int32_t& textureCount) {

			auto material = std::make_shared<Material>();
			std::string header, line;

			std::getline(stream, header);

			if (header.compare(0, 4, "AEM ") != 0) {
				return nullptr;
			}

			size_t lastPosition = 4;
			auto position = header.find_first_of('\n', lastPosition);
			textureCount = std::stoi(header.substr(lastPosition, position - lastPosition));

			std::getline(stream, line);

			lastPosition = line.find_first_of(' ');
			position = line.find_first_of("\r\n", lastPosition) - 1;
			material->name = line.substr(lastPosition + 1, position - lastPosition);

			std::getline(stream, line);

			while (line != "" && line != "\r") {
				auto prefix = line.substr(0, 2);

				if (prefix == "BC") {
					material->baseColor = ReadVector(line);
				}
				else if (prefix == "EC") {
					material->emissiveColor = ReadVector(line);
				}
				else if (prefix == "TC") {
					material->transmissiveColor = ReadVector(line);
				}
				else if (prefix == "RN") {
					material->roughness = ReadFloat(line);
				}
				else if (prefix == "MN") {
					material->metalness = ReadFloat(line);
				}
				else if (prefix == "AO") {
					material->ao = ReadFloat(line);
				}
				else if (prefix == "RF") {
					material->reflectance = ReadFloat(line);
				}
				else if (prefix == "NS") {
					material->normalScale = ReadFloat(line);
				}
				else if (prefix == "DS") {
					material->displacementScale = ReadFloat(line);
				}
				else if (prefix == "TL") {
					material->tiling = ReadFloat(line);
				}

				std::getline(stream, line);
			}

			return material;

		}

		std::string MaterialLoader::WriteVector(std::string prefix, vec3 vector) {

			return prefix + " " + std::to_string(vector.r) + " " +
				std::to_string(vector.g) + " " +
				std::to_string(vector.b) + "\n";

		}

		vec3 MaterialLoader::ReadVector(std::string line) {

			vec3 vector;

			auto lastPosition = line.find_first_of(' ') + 1;
			auto position = line.find_first_of(' ', lastPosition);
			vector.x = std::stof(line.substr(lastPosition, position - lastPosition));

			lastPosition = position + 1;
			position = line.find_first_of(' ', lastPosition);
			vector.y = std::stof(line.substr(lastPosition, position - lastPosition));

			lastPosition = position + 1;
			position = line.find_first_of("\r\n", lastPosition);
			vector.z = std::stof(line.substr(lastPosition, position - lastPosition));

			return vector;

		}

		float MaterialLoader::ReadFloat(std::string line) {

			auto lastPosition = line.find_first_of(' ') + 1;
			auto position = line.find_first_of("\r\n", lastPosition);
			return std::stof(line.substr(lastPosition, position - lastPosition));

		}

		std::string MaterialLoader::ReadFilePath(std::string line, std::string materialDirectory) {

			auto lastPosition = line.find_first_of(' ') + 1;
			auto position = line.find_first_of("\r\n", lastPosition);
			auto string = line.substr(lastPosition, position - lastPosition);
			return materialDirectory + "/" + string;

		}

    }

}