#include "ComponentSerializer.h"

#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "resource/ResourceManager.h"
#include "audio/AudioManager.h"

namespace Atlas::Scene::Components {

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
                {"radius", p.properties.point.radius},
                {"attenuation", p.properties.point.attenuation},
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
            {"shadow", *p.shadow},
            {"isMain", p.isMain},
            {"volumetric", p.volumetric}
        };
    }

    void from_json(const json& j, LightComponent& p) {
        json typeProperties;
        int type, mobility;

        p.shadow = CreateRef<Lighting::Shadow>();

        j.at("type").get_to(type);
        j.at("mobility").get_to(mobility);
        j.at("color").get_to(p.color);
        j.at("intensity").get_to(p.intensity);
        j.at("properties").get_to(typeProperties);
        j.at("shadow").get_to(*p.shadow);
        j.at("isMain").get_to(p.isMain);
        j.at("volumetric").get_to(p.volumetric);

        p.type = static_cast<LightType>(type);
        p.mobility = static_cast<LightMobility>(type);

        if (p.type == LightType::DirectionalLight) {
            typeProperties.at("direction").get_to(p.properties.directional.direction);
        }
        else if (p.type == LightType::PointLight) {
            typeProperties.at("position").get_to(p.properties.point.position);
            typeProperties.at("radius").get_to(p.properties.point.radius);
            typeProperties.at("attenuation").get_to(p.properties.point.attenuation);
        }
    }

    void to_json(json& j, const MeshComponent& p) {
        j = json {
            {"visible", p.visible},
            {"dontCull", p.dontCull}
        };

        if (p.mesh.IsValid())
            j["resourcePath"] = p.mesh.GetResource()->path;
    }

    void from_json(const json& j, MeshComponent& p) {
        j.at("visible").get_to(p.visible);
        j.at("dontCull").get_to(p.dontCull);

        if (j.contains("resourcePath")) {
            std::string resourcePath;
            j.at("resourcePath").get_to(resourcePath);

            p.mesh = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(resourcePath,
                ResourceOrigin::User, Loader::ModelLoader::LoadMesh, false, 8192);
        }

    }

    void to_json(json& j, const NameComponent& p) {
        j = json {
            {"name", p.name},
        };
    }

    void from_json(const json& j, NameComponent& p) {
        j.at("name").get_to(p.name);
    }

    void to_json(json& j, const TransformComponent& p) {
        j = json {
            {"matrix", p.matrix},
            {"isStatic", p.isStatic},
        };
    }

    void from_json(const json& j, TransformComponent& p) {
        j.at("matrix").get_to(p.matrix);
        j.at("isStatic").get_to(p.isStatic);

        // Reconstruct some immediate values, don't need to be true.
        // Just that a call to p.ReconstructLocalMatrix() before a scene updates 
        // leaves p.matrix in the same state.
        p.globalMatrix = p.matrix;
        p.lastGlobalMatrix = p.matrix;
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

        if (p.font.IsValid())
            j["resourcePath"] = p.font.GetResource()->path;
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

        if (j.contains("resourcePath")) {
            std::string resourcePath;
            j.at("resourcePath").get_to(resourcePath);

            p.font = ResourceManager<Font>::GetOrLoadResourceAsync(resourcePath,
                ResourceOrigin::User, 32.0f, 7.0f, 127);
        }
    }

    void to_json(json& j, const RigidBodyComponent& p) {
        j = json {
            {"bodyIndex", p.bodyId.GetIndex()},
            {"layer", p.layer},
        };

        j["creationSettings"] = p.GetBodyCreationSettings();
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
            {"allowInput", p.allowInput},
        };

        if (p.creationSettings != nullptr)
            j["creationSettings"] = *p.creationSettings;
    }

    void from_json(const json& j, PlayerComponent& p) {
        j.at("slowVelocity").get_to(p.slowVelocity);
        j.at("fastVelocity").get_to(p.fastVelocity);
        j.at("jumpVelocity").get_to(p.jumpVelocity);
        j.at("allowInput").get_to(p.allowInput);

        if (j.contains("creationSettings")) {
            p.creationSettings = CreateRef<Physics::PlayerCreationSettings>();
            *p.creationSettings = j["creationSettings"];
        }
    }

    void to_json(json &j, const LuaScriptComponent &p) {
        j = json{};

        if (p.script.IsValid())
            j["resourcePath"] = p.script.GetResource()->path;

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
            default:
                AE_ASSERT(false);
            }
        }
    }

    void from_json(const json &j, LuaScriptComponent &p) {

        if (j.contains("resourcePath")) {
            std::string resourcePath;
            j.at("resourcePath").get_to(resourcePath);

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
            }
        }
    }
}