#ifndef AE_SCENE_H
#define AE_SCENE_H

#include "../System.h"
#include "../actor/MeshActor.h"
#include "../terrain/Terrain.h"
#include "../lighting/Light.h"
#include "../lighting/Sky.h"
#include "../lighting/Fog.h"
#include "../lighting/IrradianceVolume.h"
#include "../lighting/AO.h"
#include "../lighting/Reflection.h"
#include "../lighting/VolumetricClouds.h"
#include "../lighting/SSS.h"
#include "../ocean/Ocean.h"
#include "../postprocessing/PostProcessing.h"
#include "../Decal.h"

#include "SceneNode.h"
#include "SpacePartitioning.h"
#include "RTData.h"
#include "Vegetation.h"

#include <unordered_map>

namespace Atlas {

    namespace Scene {

        class Scene : public SceneNode, public SpacePartitioning {

            friend class Renderer::Helper::RayTracingHelper;

        public:
            /**
             * Constructs a scene object.
             */
            Scene() : SceneNode(this, &rootMeshMap), SpacePartitioning(vec3(-2048.0f), vec3(2048.0f), 5), 
                rayTracingData(this) {}

            /**
             * Constructs a scene object.
             * @param min The minimum scene boundary.
             * @param max The maximum scene boundary.
             * @param depth The maximum depths of the octrees.
             * @note The boundary should be as tightly fitted to the scene as possible.
             * It determines the size of the octrees.
             */
            Scene(vec3 min, vec3 max, int32_t depth = 5);

            /**
             * Destructs a scene object.
             */
            ~Scene();


            Scene& operator=(const Scene& that);

            /**
             * Updates the scene based on the camera and the times passed.
             * @param camera The camera from whichs perspective the scene should be rendered later on.
             * @param deltaTime The time that has passed since the last update.
             */
            void Update(Camera *camera, float deltaTime);

            /**
             * Checks whether the scene has changed since the last update
             * @return True if scene has changed, false otherwise.
             */
            bool HasChanged();

            /**
             * Renamed from SceneNode: A scene shouldn't be transformed.
             */
            void SetMatrix() {}

            /**
             * Removes everthing from the scene.
             */
            void Clear();

            /**
             * Returns all the scenes meshes
             * @return The meshes of the scene
             */
            std::vector<Mesh::Mesh*> GetMeshes();

            /**
             * Returns all the scenes materials
             * @return The materials of the scene
             * @note The material pointers are only valid until there
             * are material changes in meshes, the terrain, etc.
             */
            std::vector<Material*> GetMaterials();

            /**
             * Builds the BVH and texture atlases.
             * @note The scene needs to be updated first.
             */
            void BuildRTStructures();

            /**
             *
             */
            void ClearRTStructures();

            /**
             * To overload the Add and Remove methods we need to specify this
             * here. It would just rename the method instead.
             */
            using SceneNode::Add;
            using SceneNode::Remove;

            Ref<Terrain::Terrain> terrain = nullptr;
            Ref<Ocean::Ocean> ocean = nullptr;
            Ref<Vegetation> vegetation = nullptr;

            Lighting::Sky sky;
            Ref<Lighting::Fog> fog = nullptr;
            Ref<Lighting::IrradianceVolume> irradianceVolume = nullptr;
            Ref<Lighting::AO> ao = nullptr;
            Ref<Lighting::Reflection> reflection = nullptr;
            Ref<Lighting::SSS> sss = nullptr;
            PostProcessing::PostProcessing postProcessing;

        private:
            RTData rayTracingData;

            std::unordered_map<Mesh::Mesh*, int32_t> rootMeshMap;

            bool hasChanged = true;

        };

    }

}

#endif