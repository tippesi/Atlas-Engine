#ifndef AE_VEGETATIONHELPER_H
#define AE_VEGETATIONHELPER_H

#include "../System.h"
#include "../../shader/Shader.h"
#include "../../scene/Vegetation.h"
#include "../../buffer/Buffer.h"

#include <map>

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			class VegetationHelper {

			public:
				VegetationHelper();

				void PrepareInstanceBuffer(Scene::Vegetation& vegetation,
					Camera* camera);

				Buffer::Buffer* GetCommandBuffer();

				size_t GetCommandBufferOffset(Mesh::VegetationMesh& mesh,
					Mesh::MeshSubData& subData);

				struct DrawElementsIndirectCommand {
					uint32_t count;
					uint32_t instanceCount;
					uint32_t firstIndex;
					uint32_t baseVertex;
					uint32_t baseInstance;
				};

			private:
				struct MeshInformation {
					vec4 aabbMin;
					vec4 aabbMax;
				};

				struct MeshSubdataInformation {
					uint32_t meshIdx;
					uint32_t indicesOffset;
					uint32_t indicesCount;
				};

				void GenerateBuffers(Scene::Vegetation& vegetation);

				void ResetCounterBuffer();

				Shader::Shader instanceCullingShader;
				Shader::Shader instanceDataShader;
				Shader::Shader instanceDrawCallShader;

				Buffer::Buffer indirectDrawCallBuffer;
				Buffer::Buffer lodCounterBuffer;

				Buffer::Buffer meshInformationBuffer;
				Buffer::Buffer meshSubdataInformationBuffer;

				std::map<Mesh::VegetationMesh*, int32_t> meshToIdxMap;
				std::vector<MeshSubdataInformation> meshSubdataInformation;

			};

		}

	}

}

#endif