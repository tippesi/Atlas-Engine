#include "ComponentSerializer.h"

#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "resource/ResourceManager.h"
#include "audio/AudioManager.h"
#include "loader/MeshLoader.h"
#include "loader/AnimationLoader.h"
#include "common/SerializationContext.h"

namespace Atlas::Scene::Components {

    static ResourceHandle<Animation::Animation> LoadAnimation(const std::string& path) {

        return ResourceManager<Animation::Animation>::GetOrLoadResourceWithLoaderAsync(path,
            ResourceOrigin::User, Loader::AnimationLoader::LoadAnimation, true);

    }

    static ResourceHandle<Mesh::Mesh> LoadMesh(const std::string& path) {

        return ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(path,
            ResourceOrigin::User, Loader::MeshLoader::LoadMesh, true);

    }

    // Number of values of a column, the first column of a component defines the count
    static size_t ColumnCount(const json& j, const char* name, size_t elementSize) {

        auto it = j.find(name);
        if (it == j.end() || !it->is_binary())
            return 0;
        return it->get_binary().size() / elementSize;

    }

    // Raw values of a column, nullptr for columns older files don't have yet
    static const uint8_t* ColumnData(const json& j, const char* name, size_t elementSize, size_t count, bool& valid) {

        auto it = j.find(name);
        if (it == j.end())
            return nullptr;
        if (it->is_binary() && it->get_binary().size() == count * elementSize)
            return it->get_binary().data();

        Log::Error(std::string("Scene column ") + name + " has the wrong size");
        valid = false;
        return nullptr;

    }

    template<class T>
    static T ColumnValue(const uint8_t* data, size_t index, T defaultValue) {

        if (!data)
            return defaultValue;
        T value;
        std::memcpy(&value, data + index * sizeof(T), sizeof(T));
        return value;

    }

    void to_json(json& j, const AnimationComponent& p) {
        j = json {
            {"time", p.time},
            {"speed", p.speed},
            {"loop", p.loop},
            {"playing", p.playing},
        };

        if (p.animation.IsValid() && !p.animation.IsGenerated())
            SerializationContext::WritePath(j, "resourcePath", p.animation.GetResource()->path);

        auto layers = json::array();
        for (const auto& layer : p.layers) {
            json layerJson = {
                {"mode", static_cast<int>(layer.mode)},
                {"time", layer.time},
                {"speed", layer.speed},
                {"loop", layer.loop},
                {"playing", layer.playing},
                {"weight", layer.GetTargetWeight()},
                {"maskRoot", layer.GetMaskRoot()},
                {"maskBlendJoints", layer.GetMaskBlendJoints()},
            };

            if (layer.animation.IsValid() && !layer.animation.IsGenerated())
                SerializationContext::WritePath(layerJson, "resourcePath", layer.animation.GetResource()->path);

            layers.push_back(layerJson);
        }
        if (!layers.empty())
            j["layers"] = layers;

        auto ikChains = json::array();
        for (const auto& chain : p.ikChains) {
            ikChains.push_back(json {
                {"upperJoint", chain.GetUpperJoint()},
                {"middleJoint", chain.GetMiddleJoint()},
                {"endJoint", chain.GetEndJoint()},
                {"target", chain.target},
                {"relativeTarget", chain.relativeTarget},
                {"weight", chain.weight},
                {"pole", chain.pole},
                {"normal", chain.normal},
                {"normalWeight", chain.normalWeight},
                {"footLock", chain.footLock},
                {"lockSpeed", chain.lockSpeed},
                {"unlockSpeed", chain.unlockSpeed},
                {"lockSpeedRatio", chain.lockSpeedRatio},
                {"lockHeight", chain.lockHeight},
                {"lockReach", chain.lockReach},
                {"lockReleaseTime", chain.lockReleaseTime},
            });
        }
        if (!ikChains.empty())
            j["ikChains"] = ikChains;
    }

    void from_json(const json& j, AnimationComponent& p) {
        j.at("time").get_to(p.time);
        j.at("speed").get_to(p.speed);
        j.at("loop").get_to(p.loop);
        j.at("playing").get_to(p.playing);

        SerializationContext::ReadHandle(j, "resourcePath", p.animation, LoadAnimation);

        if (j.contains("layers")) {
            for (const auto& layerJson : j["layers"]) {
                auto& layer = p.GetLayer(p.GetLayerCount());

                int mode = 0;
                float weight = 1.0f;
                std::string maskRoot;
                int32_t maskBlendJoints = 0;
                try_get_json(layerJson, "mode", mode);
                try_get_json(layerJson, "time", layer.time);
                try_get_json(layerJson, "speed", layer.speed);
                try_get_json(layerJson, "loop", layer.loop);
                try_get_json(layerJson, "playing", layer.playing);
                try_get_json(layerJson, "weight", weight);
                try_get_json(layerJson, "maskRoot", maskRoot);
                try_get_json(layerJson, "maskBlendJoints", maskBlendJoints);

                layer.mode = static_cast<AnimationBlendMode>(mode);
                layer.FadeTo(weight);
                layer.SetMask(maskRoot, maskBlendJoints);

                SerializationContext::ReadHandle(layerJson, "resourcePath", layer.animation, LoadAnimation);
            }
        }

        if (j.contains("ikChains")) {
            for (const auto& chainJson : j["ikChains"]) {
                auto& chain = p.GetIKChain(p.GetIKChainCount());

                std::string upperJoint, middleJoint, endJoint;
                try_get_json(chainJson, "upperJoint", upperJoint);
                try_get_json(chainJson, "middleJoint", middleJoint);
                try_get_json(chainJson, "endJoint", endJoint);
                chain.SetJoints(upperJoint, middleJoint, endJoint);

                try_get_json(chainJson, "target", chain.target);
                try_get_json(chainJson, "relativeTarget", chain.relativeTarget);
                try_get_json(chainJson, "weight", chain.weight);
                try_get_json(chainJson, "pole", chain.pole);
                try_get_json(chainJson, "normal", chain.normal);
                try_get_json(chainJson, "normalWeight", chain.normalWeight);
                try_get_json(chainJson, "footLock", chain.footLock);
                try_get_json(chainJson, "lockSpeed", chain.lockSpeed);
                try_get_json(chainJson, "unlockSpeed", chain.unlockSpeed);
                try_get_json(chainJson, "lockSpeedRatio", chain.lockSpeedRatio);
                try_get_json(chainJson, "lockHeight", chain.lockHeight);
                try_get_json(chainJson, "lockReach", chain.lockReach);
                try_get_json(chainJson, "lockReleaseTime", chain.lockReleaseTime);
            }
        }
    }

    void to_json(json& j, const AudioComponent& p) {
        j = json {
            {"falloffFactor", p.falloffFactor},
            {"falloffPower", p.falloffPower},
            {"cutoff", p.cutoff},
            {"volume", p.volume},
            {"permanentPlay", p.permanentPlay},
        };

        if (p.stream) {
            j["stream"] = *p.stream;
        }
    }

    void from_json(const json& j, AudioComponent& p) {
        p.stream = Audio::AudioManager::CreateStream(ResourceHandle<Audio::AudioData>());

        j.at("falloffFactor").get_to(p.falloffFactor);
        j.at("falloffPower").get_to(p.falloffPower);
        j.at("cutoff").get_to(p.cutoff);
        j.at("volume").get_to(p.volume);
        try_get_json(j, "permanentPlay", p.permanentPlay);

        if (j.contains("stream")) {
            j.at("stream").get_to(*p.stream);
        }
    }

    void to_json(json& j, const AudioVolumeComponent& p) {
        j = json {
            {"falloffFactor", p.falloffFactor},
            {"falloffPower", p.falloffPower},
            {"cutoff", p.cutoff},
            {"volume", p.volume},
            {"aabb", p.aabb},
            {"permanentPlay", p.permanentPlay}
        };

        if (p.stream) {
            j["stream"] = *p.stream;
        }
    }

    void from_json(const json& j, AudioVolumeComponent& p) {
        p.stream = Audio::AudioManager::CreateStream(ResourceHandle<Audio::AudioData>());

        j.at("falloffFactor").get_to(p.falloffFactor);
        j.at("falloffPower").get_to(p.falloffPower);
        j.at("cutoff").get_to(p.cutoff);
        j.at("aabb").get_to(p.aabb);
        j.at("volume").get_to(p.volume);
        try_get_json(j, "permanentPlay", p.permanentPlay);

        if (j.contains("stream")) {
            j.at("stream").get_to(*p.stream);
        }
    }

    void to_json(json& j, const FogVolumeComponent& p) {
        j = json {
            {"enable", p.enable},
            {"shape", static_cast<int32_t>(p.shape)},
            {"density", p.density},
            {"scatteringColor", p.scatteringColor},
            {"scatteringFactor", p.scatteringFactor},
            {"scatteringAnisotropy", p.scatteringAnisotropy},
            {"emissiveColor", p.emissiveColor},
            {"ambientFactor", p.ambientFactor},
            {"selfShadowing", p.selfShadowing},
            {"edgeFade", p.edgeFade},
            {"heightFalloff", p.heightFalloff},
            {"noiseAmount", p.noiseAmount},
            {"noiseScale", p.noiseScale},
            {"noiseCoverage", p.noiseCoverage},
            {"noiseSharpness", p.noiseSharpness},
            {"noiseOctaves", p.noiseOctaves},
            {"windInfluence", p.windInfluence},
            {"fadeDistance", p.fadeDistance}
        };
    }

    void from_json(const json& j, FogVolumeComponent& p) {
        auto shape = static_cast<int32_t>(p.shape);
        try_get_json(j, "shape", shape);
        p.shape = static_cast<FogVolumeShape>(shape);

        try_get_json(j, "enable", p.enable);
        try_get_json(j, "density", p.density);
        try_get_json(j, "scatteringColor", p.scatteringColor);
        try_get_json(j, "scatteringFactor", p.scatteringFactor);
        try_get_json(j, "scatteringAnisotropy", p.scatteringAnisotropy);
        try_get_json(j, "emissiveColor", p.emissiveColor);
        try_get_json(j, "ambientFactor", p.ambientFactor);
        try_get_json(j, "selfShadowing", p.selfShadowing);
        try_get_json(j, "edgeFade", p.edgeFade);
        try_get_json(j, "heightFalloff", p.heightFalloff);
        try_get_json(j, "noiseAmount", p.noiseAmount);
        try_get_json(j, "noiseScale", p.noiseScale);
        try_get_json(j, "noiseCoverage", p.noiseCoverage);
        try_get_json(j, "noiseSharpness", p.noiseSharpness);
        try_get_json(j, "noiseOctaves", p.noiseOctaves);
        try_get_json(j, "windInfluence", p.windInfluence);
        try_get_json(j, "fadeDistance", p.fadeDistance);
    }

    void to_json(json& j, const CameraComponent& p) {
        j = json {
            {"location", p.location},
            {"rotation", p.rotation},
            {"exposure", p.exposure},
            {"fieldOfView", p.fieldOfView},
            {"aspectRatio", p.aspectRatio},
            {"nearPlane", p.nearPlane},
            {"farPlane", p.farPlane},
            {"thirdPerson", p.thirdPerson},
            {"thirdPersonDistance", p.thirdPersonDistance},
            {"isMain", p.isMain},
            {"useEntityTranslation", p.useEntityTranslation},
            {"useEntityRotation", p.useEntityRotation}
        };
    }

    void from_json(const json& j, CameraComponent& p) {
        j.at("location").get_to(p.location);
        j.at("rotation").get_to(p.rotation);
        j.at("exposure").get_to(p.exposure);
        j.at("fieldOfView").get_to(p.fieldOfView);
        j.at("aspectRatio").get_to(p.aspectRatio);
        j.at("nearPlane").get_to(p.nearPlane);
        j.at("farPlane").get_to(p.farPlane);
        j.at("thirdPerson").get_to(p.thirdPerson);
        j.at("thirdPersonDistance").get_to(p.thirdPersonDistance);
        j.at("isMain").get_to(p.isMain);
        j.at("useEntityTranslation").get_to(p.useEntityTranslation);
        j.at("useEntityRotation").get_to(p.useEntityRotation);
    }

    void to_json(json& j, const LightComponent& p) {
        json typeProperties;
        if (p.type == LightType::DirectionalLight) {
            typeProperties = json {
                {"direction", p.properties.directional.direction}
            };
        }
        else if (p.type == LightType::PointLight) {
            typeProperties = json {
                {"position", p.properties.point.position},
                {"radius", p.properties.point.radius}
            };
        }
        else if (p.type == LightType::SpotLight) {
            typeProperties = json{
                {"position", p.properties.spot.position},
                {"direction", p.properties.spot.direction},
                {"radius", p.properties.spot.radius},
                {"outerConeAngle", p.properties.spot.outerConeAngle},
                {"innerConeAngle", p.properties.spot.innerConeAngle},
            };
        }

        int type = static_cast<int>(p.type);
        int mobility = static_cast<int>(p.mobility);

        j = json {
            {"type", type},
            {"mobility", mobility},
            {"color", p.color},
            {"intensity", p.intensity},
            {"properties", typeProperties},
            {"isMain", p.isMain},
            {"volumetric", p.volumetric},
            {"volumetricIntensity", p.volumetricIntensity}
        };

        if (p.shadow)
            j["shadow"] = *p.shadow;
    }

    void from_json(const json& j, LightComponent& p) {
        json typeProperties;
        int type, mobility;        

        j.at("type").get_to(type);
        j.at("mobility").get_to(mobility);
        j.at("color").get_to(p.color);
        j.at("intensity").get_to(p.intensity);
        j.at("properties").get_to(typeProperties);
        j.at("isMain").get_to(p.isMain);
        j.at("volumetric").get_to(p.volumetric);

        try_get_json(j, "volumetricIntensity", p.volumetricIntensity);

        p.type = static_cast<LightType>(type);
        p.mobility = static_cast<LightMobility>(mobility);

        if (p.type == LightType::DirectionalLight) {
            typeProperties.at("direction").get_to(p.properties.directional.direction);
        }
        else if (p.type == LightType::PointLight) {
            typeProperties.at("position").get_to(p.properties.point.position);
            typeProperties.at("radius").get_to(p.properties.point.radius);
        }
        else if (p.type == LightType::SpotLight) {
            typeProperties.at("position").get_to(p.properties.spot.position);
            typeProperties.at("direction").get_to(p.properties.spot.direction);
            typeProperties.at("radius").get_to(p.properties.spot.radius);
            typeProperties.at("outerConeAngle").get_to(p.properties.spot.outerConeAngle);
            typeProperties.at("innerConeAngle").get_to(p.properties.spot.innerConeAngle);
        }

        p.shadow = nullptr;
        if (j.contains("shadow")) {
            p.shadow = CreateRef<Lighting::Shadow>();
            j.at("shadow").get_to(*p.shadow);

            // Directional lights only have cascaded shadows, an orthographic one from an older scene becomes cascades
            if (!p.HasFittingShadow()) {
                p.FitShadowToType();
                Log::Message("Converted a shadow which didn't fit its light, e.g. the orthographic shadow of a directional light");
            }
        }
    }

    void to_json(json& j, const MeshComponent& p) {
        j = json {
            {"visible", p.visible},
            {"dontCull", p.dontCull}
        };

        if (p.mesh.IsValid() && !p.mesh.IsGenerated())
            SerializationContext::WritePath(j, "resourcePath", p.mesh.GetResource()->path);
    }

    void from_json(const json& j, MeshComponent& p) {
        j.at("visible").get_to(p.visible);
        j.at("dontCull").get_to(p.dontCull);

        SerializationContext::ReadHandle(j, "resourcePath", p.mesh, LoadMesh);

    }

    void ComponentColumns<MeshComponent>::Write(json& j, size_t count, const std::function<const MeshComponent&(size_t)>& get) {
        std::vector<uint8_t> resources(count * sizeof(uint32_t));
        std::vector<uint8_t> flags(count);
        for (size_t i = 0; i < count; i++) {
            const auto& p = get(i);
            auto hasResource = p.mesh.IsValid() && !p.mesh.IsGenerated();
            auto resource = hasResource ? SerializationContext::WriteIndex(p.mesh.GetResource()->path) : UINT32_MAX;
            std::memcpy(resources.data() + i * sizeof(uint32_t), &resource, sizeof(uint32_t));
            flags[i] = uint8_t(p.visible ? 1 : 0) | uint8_t(p.dontCull ? 2 : 0);
        }
        j["resource"] = json::binary_t(std::move(resources));
        j["flags"] = json::binary_t(std::move(flags));
    }

    template<class Func>
    static bool ReadMeshColumns(const json& j, Func&& func) {
        auto count = ColumnCount(j, "resource", sizeof(uint32_t));
        auto valid = true;
        auto resources = ColumnData(j, "resource", sizeof(uint32_t), count, valid);
        auto flags = ColumnData(j, "flags", sizeof(uint8_t), count, valid);
        if (!valid)
            return false;
        for (size_t i = 0; i < count; i++) {
            auto flag = ColumnValue<uint8_t>(flags, i, 1);
            func(ColumnValue<uint32_t>(resources, i, UINT32_MAX), (flag & 1) != 0, (flag & 2) != 0);
        }
        return true;
    }

    bool ComponentColumns<MeshComponent>::Read(const json& j, std::vector<MeshComponent>& components) {
        return ReadMeshColumns(j, [&](uint32_t resource, bool visible, bool dontCull) {
            auto& p = components.emplace_back();
            p.visible = visible;
            p.dontCull = dontCull;
            if (resource != UINT32_MAX)
                SerializationContext::ReadHandle(resource, p.mesh, LoadMesh);
        });
    }

    json ComponentColumns<MeshComponent>::ToValues(const json& j) {
        // Without loading the meshes: the values keep the index into the resource table
        json values = json::array();
        ReadMeshColumns(j, [&](uint32_t resource, bool visible, bool dontCull) {
            json value = { {"visible", visible}, {"dontCull", dontCull} };
            if (resource != UINT32_MAX)
                value["resourcePath"] = resource;
            values.push_back(std::move(value));
        });
        return values;
    }

    void to_json(json& j, const NameComponent& p) {
        j = json {
            {"name", p.name},
        };
    }

    void from_json(const json& j, NameComponent& p) {
        j.at("name").get_to(p.name);
    }

    void ComponentColumns<NameComponent>::Write(json& j, size_t count, const std::function<const NameComponent&(size_t)>& get) {
        // All names in one buffer, the offsets mark where each one starts
        std::vector<uint8_t> text;
        std::vector<uint8_t> offsets((count + 1) * sizeof(uint32_t));
        for (size_t i = 0; i < count; i++) {
            const auto& name = get(i).name;
            text.insert(text.end(), name.begin(), name.end());
            auto offset = uint32_t(text.size());
            std::memcpy(offsets.data() + (i + 1) * sizeof(uint32_t), &offset, sizeof(uint32_t));
        }
        j["text"] = json::binary_t(std::move(text));
        j["offset"] = json::binary_t(std::move(offsets));
    }

    bool ComponentColumns<NameComponent>::Read(const json& j, std::vector<NameComponent>& components) {
        std::vector<uint32_t> offsets;
        try_get_binary_json(j, "offset", offsets);
        if (offsets.empty())
            return true;

        auto textIt = j.find("text");
        if (textIt == j.end() || !textIt->is_binary()) {
            Log::Error("Scene name column without text");
            return false;
        }
        const auto& text = textIt->get_binary();
        for (size_t i = 0; i + 1 < offsets.size(); i++) {
            if (offsets[i] > offsets[i + 1] || offsets[i + 1] > text.size()) {
                Log::Error("Scene name column has invalid offsets");
                return false;
            }
        }

        components.resize(offsets.size() - 1);
        for (size_t i = 0; i < components.size(); i++)
            components[i].name.assign(reinterpret_cast<const char*>(text.data()) + offsets[i], offsets[i + 1] - offsets[i]);
        return true;
    }

    json ComponentColumns<NameComponent>::ToValues(const json& j) {
        std::vector<NameComponent> components;
        Read(j, components);
        json values = json::array();
        for (const auto& p : components)
            values.push_back(p);
        return values;
    }

    void to_json(json& j, const TransformComponent& p) {
        j = json {
            {"isStatic", p.isStatic},
            {"parentJoint", p.parentJoint},
        };
        SerializationContext::WriteMatrix(j, "matrix", p.matrix);
    }

    void from_json(const json& j, TransformComponent& p) {
        j.at("matrix").get_to(p.matrix);
        j.at("isStatic").get_to(p.isStatic);
        try_get_json(j, "parentJoint", p.parentJoint);

        // Reconstruct some immediate values, don't need to be true.
        // Just that a call to p.ReconstructLocalMatrix() before a scene updates
        // leaves p.matrix in the same state.
        p.globalMatrix = p.matrix;
        p.lastGlobalMatrix = p.matrix;
    }

    void ComponentColumns<TransformComponent>::Write(json& j, size_t count, const std::function<const TransformComponent&(size_t)>& get) {
        std::vector<uint8_t> matrices(count * sizeof(mat4));
        std::vector<uint8_t> isStatic(count);
        std::vector<uint8_t> parentJoints(count * sizeof(int32_t));
        for (size_t i = 0; i < count; i++) {
            const auto& p = get(i);
            std::memcpy(matrices.data() + i * sizeof(mat4), &p.matrix[0][0], sizeof(mat4));
            isStatic[i] = p.isStatic ? 1 : 0;
            std::memcpy(parentJoints.data() + i * sizeof(int32_t), &p.parentJoint, sizeof(int32_t));
        }
        j["matrix"] = json::binary_t(std::move(matrices));
        j["isStatic"] = json::binary_t(std::move(isStatic));
        j["parentJoint"] = json::binary_t(std::move(parentJoints));
    }

    bool ComponentColumns<TransformComponent>::Read(const json& j, std::vector<TransformComponent>& components) {
        auto count = ColumnCount(j, "matrix", sizeof(mat4));
        auto valid = true;
        auto matrices = ColumnData(j, "matrix", sizeof(mat4), count, valid);
        auto isStatic = ColumnData(j, "isStatic", sizeof(uint8_t), count, valid);
        auto parentJoints = ColumnData(j, "parentJoint", sizeof(int32_t), count, valid);
        if (!valid)
            return false;

        components.resize(count);
        for (size_t i = 0; i < count; i++) {
            auto& p = components[i];
            std::memcpy(&p.matrix[0][0], matrices + i * sizeof(mat4), sizeof(mat4));
            p.isStatic = ColumnValue<uint8_t>(isStatic, i, 1) != 0;
            p.parentJoint = ColumnValue<int32_t>(parentJoints, i, -1);
            // Like from_json
            p.globalMatrix = p.matrix;
            p.lastGlobalMatrix = p.matrix;
        }
        return true;
    }

    json ComponentColumns<TransformComponent>::ToValues(const json& j) {
        std::vector<TransformComponent> components;
        Read(j, components);
        json values = json::array();
        for (const auto& p : components)
            values.push_back(p);
        return values;
    }

    void to_json(json& j, const TextComponent& p) {
        j = json {
            {"text", p.text},
            {"position", p.position},
            {"rotation", p.rotation},
            {"halfSize", p.halfSize},
            {"textColor", p.textColor},
            {"outlineColor", p.outlineColor},
            {"outlineFactor", p.outlineFactor},
            {"textScale", p.textScale},
        };

        if (p.font.IsValid() && !p.font.IsGenerated())
            SerializationContext::WritePath(j, "resourcePath", p.font.GetResource()->path);
    }

    void from_json(const json& j, TextComponent& p) {
        j.at("text").get_to(p.text);
        j.at("position").get_to(p.position);
        j.at("rotation").get_to(p.rotation);
        j.at("halfSize").get_to(p.halfSize);
        j.at("textColor").get_to(p.textColor);
        j.at("outlineColor").get_to(p.outlineColor);
        j.at("outlineFactor").get_to(p.outlineFactor);
        j.at("textScale").get_to(p.textScale);

        std::string resourcePath;
        if (SerializationContext::ReadPath(j, "resourcePath", resourcePath)) {
            p.font = ResourceManager<Font>::GetOrLoadResourceAsync(resourcePath,
                ResourceOrigin::User, 32.0f, 7.0f, 127);
        }
    }

    void to_json(json& j, const RigidBodyComponent& p) {
        auto creationSettings = p.GetBodyCreationSettings();

        // The creation settings are the source of truth, e.g. the editor only changes those
        j = json {
            {"bodyIndex", p.bodyId.GetIndex()},
            {"layer", creationSettings.objectLayer},
        };

        j["creationSettings"] = creationSettings;
    }

    void from_json(const json& j, RigidBodyComponent& p) {
        // This whole thing works because only components with physics world are considered valid,
        // so the body id will be overriden at some point.
        uint32_t bodyIndex;
        j.at("bodyIndex").get_to(bodyIndex);
        j.at("layer").get_to(p.layer);

        // This is only relevant for not yet created bodies
        if (j.contains("creationSettings")) {
            p.creationSettings = CreateRef<Physics::BodyCreationSettings>();
            *p.creationSettings = j["creationSettings"];
            p.layer = p.creationSettings->objectLayer;
        }

        // We can use the index here since when loading nothing but the loading thread
        // will access the physics system (in multithreaded scenarios we would need to use
        // the sequence number as well
        p.bodyId = Physics::BodyID(bodyIndex);
    }

    void to_json(json& j, const PlayerComponent& p) {
        j = json {
            {"slowVelocity", p.slowVelocity},
            {"fastVelocity", p.fastVelocity},
            {"jumpVelocity", p.jumpVelocity},
            {"acceleration", p.acceleration},
            {"deceleration", p.deceleration},
            {"allowInput", p.allowInput},
            {"crouchHeight", p.crouchHeight},
            {"crouchVelocity", p.crouchVelocity},
            {"walkStairStepUpDistance", p.walkStairStepUpDistance},
        };

        if (p.creationSettings != nullptr)
            j["creationSettings"] = *p.creationSettings;
    }

    void from_json(const json& j, PlayerComponent& p) {
        j.at("slowVelocity").get_to(p.slowVelocity);
        j.at("fastVelocity").get_to(p.fastVelocity);
        j.at("jumpVelocity").get_to(p.jumpVelocity);
        j.at("allowInput").get_to(p.allowInput);
        try_get_json(j, "acceleration", p.acceleration);
        try_get_json(j, "deceleration", p.deceleration);
        try_get_json(j, "crouchHeight", p.crouchHeight);
        try_get_json(j, "crouchVelocity", p.crouchVelocity);
        try_get_json(j, "walkStairStepUpDistance", p.walkStairStepUpDistance);

        if (j.contains("creationSettings")) {
            p.creationSettings = CreateRef<Physics::PlayerCreationSettings>();
            *p.creationSettings = j["creationSettings"];
        }
    }

    void to_json(json& j, const ConstraintComponent& p) {
        j = json {
            {"creationSettings", p.creationSettings},
            {"connectedEntity", size_t(p.connectedEntity)},
        };
    }

    void from_json(const json& j, ConstraintComponent& p) {
        j.at("creationSettings").get_to(p.creationSettings);
        size_t connectedEntity;
        j.at("connectedEntity").get_to(connectedEntity);
        p.connectedEntity = connectedEntity;
    }

    void to_json(json &j, const LuaScriptComponent &p) {
        j = json{};

        if (p.script.IsValid() && !p.script.IsGenerated())
            SerializationContext::WritePath(j, "resourcePath", p.script.GetResource()->path);

        j["permanentExecution"] = p.permanentExecution;

        for (const auto& [name, prop] : p.properties) {
            switch (prop.type) {
            case LuaScriptComponent::PropertyType::Boolean:
                j["scriptProperties"][name]["type"] = "boolean";
                j["scriptProperties"][name]["value"] = prop.booleanValue;
                break;
            case LuaScriptComponent::PropertyType::Integer:
                j["scriptProperties"][name]["type"] = "integer";
                j["scriptProperties"][name]["value"] = prop.integerValue;
                break;
            case LuaScriptComponent::PropertyType::Double:
                j["scriptProperties"][name]["type"] = "double";
                j["scriptProperties"][name]["value"] = prop.doubleValue;
                break;
            case LuaScriptComponent::PropertyType::String:
                j["scriptProperties"][name]["type"] = "string";
                j["scriptProperties"][name]["value"] = prop.stringValue;
                break;
            case LuaScriptComponent::PropertyType::Vec2:
                j["scriptProperties"][name]["type"] = "vec2";
                j["scriptProperties"][name]["value"] = prop.vec2Value;
                break;
            case LuaScriptComponent::PropertyType::Vec3:
                j["scriptProperties"][name]["type"] = "vec3";
                j["scriptProperties"][name]["value"] = prop.vec3Value;
                break;
            case LuaScriptComponent::PropertyType::Vec4:
                j["scriptProperties"][name]["type"] = "vec4";
                j["scriptProperties"][name]["value"] = prop.vec4Value;
                break;
            default:
                break;
            }
        }
    }

    void from_json(const json &j, LuaScriptComponent &p) {

        std::string resourcePath;
        if (SerializationContext::ReadPath(j, "resourcePath", resourcePath)) {
            try_get_json(j, "permanentExecution", p.permanentExecution);

            p.script = ResourceManager<Scripting::Script>::GetOrLoadResourceAsync(
                resourcePath, ResourceOrigin::User);
        }

        if (j.contains("scriptProperties"))
        {
            for (const auto &v : j["scriptProperties"].items())
            {
                const auto &value = v.value();
                auto propertyTypeAsString = value["type"].get<std::string>();
                if (propertyTypeAsString == "boolean")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<bool>());
                }
                else if (propertyTypeAsString == "integer")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<int32_t>());
                }
                else if (propertyTypeAsString == "double")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<double>());
                }
                else if (propertyTypeAsString == "string")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<std::string>());
                }
                else if (propertyTypeAsString == "vec2")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<vec2>());
                }
                else if (propertyTypeAsString == "vec3")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<vec3>());
                }
                else if (propertyTypeAsString == "vec4")
                {
                    p.SetPropertyValue(v.key(), value["value"].get<vec4>());
                }
            }
        }
    }

    void to_json(json& j, const SplineComponent& p) {

        int type = static_cast<int>(p.type);
        int bakeMode = static_cast<int>(p.bakeMode);

        j = json{
            {"type", type},
            {"bakeMode", bakeMode}
        };

        auto& controlPoints = j["controlPoints"];
        for (const auto& controlPoint : p.controlPoints) {
            controlPoints.emplace_back(json {
                {"transform", controlPoint.transform},
                {"tangent", controlPoint.tangent},
                {"time", controlPoint.time}
                });
        }
    }

    void from_json(const json& j, SplineComponent& p) {
        json typeProperties;
        int type, bakeMode;

        j.at("type").get_to(type);
        j.at("bakeMode").get_to(bakeMode);

        p.type = static_cast<SplineType>(type);
        p.bakeMode = static_cast<SplineBakeMode>(bakeMode);

        const auto& controlPoints = j["controlPoints"];
        for (const auto& cp : controlPoints) {
            p.controlPoints.emplace_back(cp["transform"], cp["time"]);
            p.controlPoints.back().tangent = cp["tangent"];
        }
    }
}
