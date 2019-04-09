#include "MaterialLoader.h"
#include "AssetLoader.h"
#include "../common/Path.h"

#include "../libraries/stb/stb_image.h"
#include "../libraries/stb/stb_image_write.h"

namespace Atlas {

    namespace Loader {

        Material* MaterialLoader::LoadMaterial(std::string filename) {

            auto stream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!stream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to load material %s", filename.c_str());
#endif
                throw AtlasException("Material couldn't be loaded");
            }

			auto material = new Material();
			std::string header, line;

			std::getline(stream, header);

			if (header.compare(0, 4, "AEM ") != 0) {
				throw AtlasException("File isn't a material file");
			}

			size_t lastPosition = 4;
			auto position = header.find_first_of('\n', lastPosition);
			auto textureCount = std::stoi(header.substr(lastPosition, position - lastPosition));

			std::getline(stream, line);

			lastPosition = line.find_first_of(' ');
			position = line.find_first_of("\r\n", lastPosition);
			material->name = line.substr(lastPosition + 1, position - lastPosition);

			std::getline(stream, line);
			material->diffuseColor = ReadVector(line);

			std::getline(stream, line);
			material->specularColor = ReadVector(line);

			std::getline(stream, line);
			material->ambientColor = ReadVector(line);

			std::getline(stream, line);
			material->specularHardness = ReadFloat(line);

			std::getline(stream, line);
			material->specularIntensity = ReadFloat(line);

			std::getline(stream, line);
			material->displacementScale = ReadFloat(line);

			auto materialDirectory = Common::Path::GetDirectory(AssetLoader::GetFullPath(filename));

			for (int32_t i = 0; i < textureCount; i++) {
				std::getline(stream, line);
				auto prefix = line.substr(0, 3);
				if (prefix == "AMP") {
					material->diffuseMapPath = ReadFilePath(line, materialDirectory);
					material->diffuseMap = new Texture::Texture2D(material->diffuseMapPath);
				}
				else if (prefix == "NMP") {
					material->normalMapPath = ReadFilePath(line, materialDirectory);
					material->normalMap = new Texture::Texture2D(material->normalMapPath);
				}
				else if (prefix == "SMP") {
					material->specularMapPath = ReadFilePath(line, materialDirectory);
					material->specularMap = new Texture::Texture2D(material->specularMapPath);
				}
				else if (prefix == "DMP") {
					material->displacementMapPath = ReadFilePath(line, materialDirectory);
					material->displacementMap = new Texture::Texture2D(material->displacementMapPath);
				}
			}

            stream.close();

			return material;

        }

        void MaterialLoader::SaveMaterial(Material *material, std::string filename) {

            auto stream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!stream.is_open()) {
#ifdef AE_SHOW_LOG
                AtlasLog("Failed to save material %s", filename.c_str());
#endif
                throw AtlasException("Material couldn't be loaded");
            }

			std::string header, body;
			int32_t textureCount = 0;

			header.append("AEM ");

			textureCount = material->HasDiffuseMap() ? textureCount + 1 : textureCount;
			textureCount = material->HasNormalMap() ? textureCount + 1 : textureCount;
			textureCount = material->HasSpecularMap() ? textureCount + 1 : textureCount;
			textureCount = material->HasDisplacementMap() ? textureCount + 1 : textureCount;

			header.append(std::to_string(textureCount) + "\n");

			stream << header;

			body.append("N " + material->name + "\n");

            body.append(WriteVector("DC", material->diffuseColor));
			body.append(WriteVector("SC", material->specularColor));
			body.append(WriteVector("AC", material->ambientColor));

            body.append("SH " + std::to_string(material->specularHardness) + "\n");
            body.append("SI " + std::to_string(material->specularIntensity) + "\n");
            body.append("DS " + std::to_string(material->displacementScale) + "\n");

            stream << body;

			auto materialPath = AssetLoader::GetFullPath(filename);

			if (material->HasDiffuseMap())
				stream << "AMP " + Common::Path::GetRelative(materialPath, material->diffuseMapPath) + "\n";
			if (material->HasNormalMap())
				stream << "NMP " + Common::Path::GetRelative(materialPath, material->normalMapPath) + "\n";
			if (material->HasSpecularMap())
				stream << "SMP " + Common::Path::GetRelative(materialPath, material->specularMapPath) + "\n";
			if (material->HasDisplacementMap())
				stream << "DMP " + Common::Path::GetRelative(materialPath, material->displacementMapPath) + "\n";

            stream.close();

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
			return Common::Path::GetAbsolute(materialDirectory + "/" + string);

		}

    }

}