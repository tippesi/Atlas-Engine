#include "Material.h"
#include "renderer/OpaqueRenderer.h"
#include "renderer/ShadowRenderer.h"

namespace Atlas {

	namespace Mesh {

		Material::Material() {

			arrayMap = nullptr;
			diffuseMap = nullptr;
			normalMap = nullptr;
			specularMap = nullptr;
			displacementMap = nullptr;

			diffuseColor = vec3(1.0f);
			specularColor = vec3(1.0f);
			ambientColor = vec3(0.2f);

			specularHardness = 1.0f;
			specularIntensity = 0.0f;

			displacementScale = 1.0f;

			hasDiffuseMap = false;
			hasNormalMap = false;
			hasSpecularMap = false;
			hasDisplacementMap = false;

		}

		Material::~Material() {

			delete diffuseMap;
			delete normalMap;
			delete specularMap;
			delete displacementMap;

		}

		void Material::Update() {

			int32_t width = 0, height = 0, channels = 0;

			Renderer::OpaqueRenderer::RemoveConfig(&geometryConfig);
			Renderer::ShadowRenderer::RemoveConfig(&shadowConfig);

			geometryConfig.ClearMacros();
			shadowConfig.ClearMacros();

			bool useArrayMap = true;

			// Check if we can create an Texture2dArray for the material
			for (auto& imageKey : images) {
				auto image = &imageKey.second;
				if (channels == 0) {
					width = image->width;
					height = image->height;
					channels = image->channels;
				}
				else {
					if (image->width != width ||
						image->height != height ||
						image->channels != channels)
						useArrayMap = false;
				}
			}

			// Create the textures
			if (useArrayMap && images.size() > 0) {
				arrayMap = new Texture::Texture2DArray(GL_UNSIGNED_BYTE, width, height, (int32_t)images.size(),
													   Texture::Texture::GetSuggestedFormat(channels), GL_CLAMP_TO_EDGE, GL_LINEAR, true, true);
			}

			int32_t index = 0;

			for (auto& imageKey : images) {
				Loader::Image* image = &imageKey.second;
				int32_t map = imageKey.first;
				if (useArrayMap) {
					switch(map) {
						case AE_MATERIAL_DIFFUSE_MAP: diffuseMapIndex = (float)index; break;
						case AE_MATERIAL_NORMAL_MAP: normalMapIndex = (float)index; break;
						case AE_MATERIAL_SPECULAR_MAP: specularMapIndex = (float)index; break;
						case AE_MATERIAL_DISPLACEMENT_MAP: displacementMapIndex = (float)index; break;
					}
					arrayMap->SetData(image->data, index++);
				} else {
					auto texture = new Texture::Texture2D(GL_UNSIGNED_BYTE, image->width, image->height,
														  Texture::Texture::GetSuggestedFormat(image->channels), GL_CLAMP_TO_EDGE, GL_LINEAR, true, true);
					switch(map) {
						case AE_MATERIAL_DIFFUSE_MAP: diffuseMap = texture; break;
						case AE_MATERIAL_NORMAL_MAP: normalMap = texture; break;
						case AE_MATERIAL_SPECULAR_MAP: specularMap = texture; break;
						case AE_MATERIAL_DISPLACEMENT_MAP: displacementMap = texture; break;
					}
					texture->SetData(image->data);
				}
			}

			images.clear();

			if (HasArrayMap()) {
				geometryConfig.AddMacro("ARRAY_MAP");
				shadowConfig.AddMacro("ARRAY_MAP");
			}

			if (HasDiffuseMap()) {
				geometryConfig.AddMacro("DIFFUSE_MAP");
				if (!HasArrayMap()) {
					if (diffuseMap->channels == 4) {
						shadowConfig.AddMacro("ALPHA");
					}
				}
				else {
					if (arrayMap->channels == 4) {
						shadowConfig.AddMacro("ALPHA");
					}
				}
			}

			if (HasNormalMap()) {
				geometryConfig.AddMacro("NORMAL_MAP");
			}

			Renderer::OpaqueRenderer::AddConfig(&geometryConfig);
			Renderer::ShadowRenderer::AddConfig(&shadowConfig);

		}

		void Material::AddDiffuseMap(Loader::Image image) {

			hasDiffuseMap = true;
			images[AE_MATERIAL_DIFFUSE_MAP] = image;

		}

		void Material::AddNormalMap(Loader::Image image) {

			hasNormalMap = true;
			images[AE_MATERIAL_NORMAL_MAP] = image;

		}

		void Material::AddSpecularMap(Loader::Image image) {

			hasSpecularMap = true;
			images[AE_MATERIAL_SPECULAR_MAP] = image;

		}

		void Material::AddDisplacementMap(Loader::Image image) {

			hasDisplacementMap = true;
			images[AE_MATERIAL_DISPLACEMENT_MAP] = image;

		}

		bool Material::HasArrayMap() {

			if (arrayMap == nullptr)
				return false;

			return true;

		}

		bool Material::HasDiffuseMap() {

			return hasDiffuseMap;

		}

		bool Material::HasNormalMap() {

			return hasNormalMap;

		}

		bool Material::HasSpecularMap() {

			return hasSpecularMap;

		}

		bool Material::HasDisplacementMap() {

			return hasDisplacementMap;

		}

		float Material::GetDiffuseMapIndex() {

			return diffuseMapIndex;

		}

		float Material::GetNormalMapIndex() {

			return normalMapIndex;

		}

		float Material::GetSpecularMapIndex() {

			return specularMapIndex;

		}

		float Material::GetDisplacementMapIndex() {

			return displacementMapIndex;

		}

	}

}