#ifndef AE_MATERIAL_H
#define AE_MATERIAL_H

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/VulkanTexture.h"
#include "texture/Texture2DArray.h"
#include "loader/ImageLoader.h"
#include "shader/Shader.h"
#include "shader/ShaderConfig.h"
#include "common/Ref.h"

#include <string>

namespace Atlas {

	class Material {

	public:
		Material();

		Material(const Material& that);

		~Material();

		Material& operator=(const Material& that);

		bool HasBaseColorMap() const;
		bool HasOpacityMap() const;
		bool HasNormalMap() const;
		bool HasRoughnessMap() const;
		bool HasMetalnessMap() const;
		bool HasAoMap() const;
		bool HasDisplacementMap() const;

		std::string name;

		Texture::Texture2D* baseColorMap = nullptr;
		Texture::Texture2D* opacityMap = nullptr;
		Texture::Texture2D* normalMap = nullptr;
		Texture::Texture2D* roughnessMap = nullptr;
		Texture::Texture2D* metalnessMap = nullptr;
		Texture::Texture2D* aoMap = nullptr;
		Texture::Texture2D* displacementMap = nullptr;

		vec3 baseColor = vec3(1.0f);
		vec3 emissiveColor = vec3(0.0f);
		vec3 transmissiveColor = vec3(0.0f);

		float opacity = 1.0f;

		float roughness = 1.0f;
		float metalness = 0.0f;
		float ao = 1.0f;

		float reflectance = 0.5f;

		float normalScale = 0.5f;
		float displacementScale = 0.01f;

		float tiling = 1.0f;

		std::string baseColorMapPath;
		std::string opacityMapPath;
		std::string normalMapPath;
		std::string roughnessMapPath;
		std::string metalnessMapPath;
		std::string aoMapPath;
		std::string displacementMapPath;

		bool twoSided = false;

	private:
		void DeepCopy(const Material& that);

		void DeleteTextures();

	};

    class VulkanMaterial {

    public:
        VulkanMaterial();

        VulkanMaterial(const VulkanMaterial& that);

        ~VulkanMaterial();

        VulkanMaterial& operator=(const VulkanMaterial& that);

        bool HasBaseColorMap() const;
        bool HasOpacityMap() const;
        bool HasNormalMap() const;
        bool HasRoughnessMap() const;
        bool HasMetalnessMap() const;
        bool HasAoMap() const;
        bool HasDisplacementMap() const;

        std::string name;

        Ref<Texture::Texture2D> baseColorMap = nullptr;
        Ref<Texture::Texture2D> opacityMap = nullptr;
        Ref<Texture::Texture2D> normalMap = nullptr;
        Ref<Texture::Texture2D> roughnessMap = nullptr;
        Ref<Texture::Texture2D> metalnessMap = nullptr;
        Ref<Texture::Texture2D> aoMap = nullptr;
        Ref<Texture::Texture2D> displacementMap = nullptr;

        vec3 baseColor = vec3(1.0f);
        vec3 emissiveColor = vec3(0.0f);
        vec3 transmissiveColor = vec3(0.0f);

        float opacity = 1.0f;

        float roughness = 1.0f;
        float metalness = 0.0f;
        float ao = 1.0f;

        float reflectance = 0.5f;

        float normalScale = 0.5f;
        float displacementScale = 0.01f;

        float tiling = 1.0f;

        std::string baseColorMapPath;
        std::string opacityMapPath;
        std::string normalMapPath;
        std::string roughnessMapPath;
        std::string metalnessMapPath;
        std::string aoMapPath;
        std::string displacementMapPath;

        bool twoSided = false;

    private:
        void DeepCopy(const VulkanMaterial& that);

    };


}

#endif