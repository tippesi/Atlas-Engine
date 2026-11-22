#pragma once

#include "../System.h"
#include "Renderer.h"
#include "OpaqueRenderer.h"
#include "helper/RenderList.h"

namespace Atlas {

    namespace Renderer {

        // Draws the translucent sub data of the main pass one instance after another from far to near, blended in a forward
        // pass after the ocean. The closest alpha blended surfaces are written into a front layer before, the reflections
        // start on them (see RTReflectionRenderer::RenderReflectionData). Refracting surfaces trace the background in screen
        // space, through their back faces
        class TranslucentRenderer : public Renderer {

        public:
            TranslucentRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            // Returns whether a surface was drawn into the front layer, without one it's left out of the reflections
            bool RenderFrontLayer(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap);

            // Returns whether any surface was drawn, its coverage is in the translucency texture then. With reflections the
            // surfaces read the ones traced from the front layer where they are its closest surface
            bool Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap, bool reflections);

            // Depth, normals, roughness, velocity and material of the closest alpha blended surfaces where they are in
            // front of the G-buffer, the depth is the one of the G-buffer elsewhere
            RenderTargetData frontLayer;

            // The coverage of the blended surfaces, it makes them reactive for the upscalers
            Texture::Texture2D translucencyTexture;

        private:
            enum class DrawPass {
                FrontLayer = 0,
                Blended,
                RefractionBackFaces
            };

            // Has to match deferred/geometry.hsh with TRANSLUCENT_PASS
            struct alignas(16) PushConstants {
                OpaqueRenderer::DrawData draw;
                uint32_t deformOffset;
                uint32_t jointCount;
                uint32_t instanceOffset;
                uint32_t firstDraw;
                // 1 without refraction
                float refractiveIndex;
                float thickness;
                uint32_t materialGraphOffset;
            };

            // Has to match translucent/translucent.hsh
            struct alignas(16) Uniforms {
                vec2 resolution;
                vec2 froxelGridScale;
                int32_t lightBucketCount;
                int32_t lightTileCountX;
                int32_t specularProbeMipLevels;
                int32_t refractionMipLevels;
                float froxelNear;
                float froxelFar;
                float froxelExtendedFar;
                int32_t froxelSliceCount;
                float froxelDistributionExponent;
            };

            // The pipeline of the sub data in the pass, invalid while the one of its graph is compiled. Back faces of two
            // sided surfaces are blended before their front faces
            PipelineConfig GetPipelineConfig(Mesh::MeshSubData* subData, Mesh::Mesh* mesh, const Ref<RenderTarget>& target,
                DrawPass drawPass, bool backFaces, bool softBody, bool reversedWinding, bool& usesGraph);

            // Calls drawSubData(instance, subData, mesh, backFaces) for the sub data drawn by the pass, in draw order
            template<class DrawSubData>
            void ForEachDraw(RenderList* renderList, const RenderList::Pass& pass, DrawPass drawPass, const DrawSubData& drawSubData);

            // Draws the sub data of the pass into the rendering which has begun
            void Draw(DrawPass drawPass, const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
                Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass& pass,
                std::unordered_map<void*, uint16_t>& materialMap);

            void BindPassBuffers(Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList,
                const RenderList::Pass& pass);

            void CullLights(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            // Returns whether the refraction textures were created again, they need to be written once then
            bool ResizeRefractionTextures(const Ref<RenderTarget>& target);

            void CopyBackground(Ref<RenderTarget> target, Graphics::CommandList* commandList);

            Graphics::RenderingDesc frontLayerRendering;
            Graphics::RenderingDesc translucentRendering;

            // The background seen through refracting surfaces with mips for the rough ones, and its depth
            Texture::Texture2D refractionTexture;
            Texture::Texture2D refractionDepthTexture;
            // The closest back faces of refracting surfaces with their normals, where the rays leave them
            Texture::Texture2D refractionBackDepthTexture;
            Texture::Texture2D refractionBackNormalTexture;
            Graphics::RenderingDesc refractionBackRendering;

            // Macros of the scene features the blended pass reads, set for each frame
            std::vector<std::string> sceneMacros;
            Hash sceneMacrosHash = 0;
            bool screenSpaceRefraction = false;

            PipelineConfig lightCullingPipelineConfig;
            Buffer::Buffer lightCullingBuffer;
            ivec2 lightTileCount = ivec2(0);
            Buffer::UniformBuffer uniformBuffer;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}
