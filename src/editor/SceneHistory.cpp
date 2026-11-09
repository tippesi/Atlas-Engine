#include "SceneHistory.h"

#include "scene/SceneSerializer.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Atlas::Editor {

    using namespace Scene::Components;

    void SceneHistory::Update(const Ref<Scene::Scene>& scene, const std::unordered_set<ECS::Entity>& selectedEntities) {

        this->scene = scene;

        auto interactionFinished = history.IsInteractionFinished();

        if (interactionFinished && !settingsState.is_null()) {
            auto settings = SaveSettings();
            if (!IsSameSettings(settingsState, settings)) {
                auto before = settingsState;
                history.Push({
                    .name = "Edit scene settings",
                    .undo = [this, before]() { RestoreSettings(before); },
                    .redo = [this, settings]() { RestoreSettings(settings); }
                });
                settingsState = settings;
            }
        }

        if (settingsState.is_null())
            settingsState = SaveSettings();

        // Entities which were deselected in the frame the interaction finished still need to be compared,
        // e.g. a text field is confirmed by clicking on another entity
        std::vector<EntityChange> changes;
        if (interactionFinished) {
            for (auto& [ecsEntity, state] : entityStates) {
                Scene::Entity entity(ecsEntity, &scene->entityManager);
                if (!entity.IsValid())
                    continue;

                // Even without a change the state is updated, such that rounding errors don't add up
                auto current = SaveComponents(entity);
                if (!IsSameEntity(state, current))
                    changes.push_back({ ecsEntity, state, current });
                state = current;
            }
            std::erase_if(entityStates, [&](const auto& state) { return !selectedEntities.contains(state.first); });
        }

        for (auto ecsEntity : selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (entity.IsValid() && !entityStates.contains(ecsEntity))
                entityStates[ecsEntity] = SaveComponents(entity);
        }

        if (changes.empty())
            return;

        auto nameComponent = changes.size() == 1 ? Scene::Entity(changes.front().entity, &scene->entityManager)
            .TryGetComponent<NameComponent>() : nullptr;
        auto name = changes.size() > 1 ? "Edit " + std::to_string(changes.size()) + " entities" :
            nameComponent ? "Edit " + nameComponent->name : std::string("Edit entity");
        PushEntityChanges(name, changes);

    }

    void SceneHistory::BeginChange(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& roots) {

        this->scene = scene;

        hierarchiesBefore = SaveHierarchies(roots);
        changeStarted = true;

    }

    void SceneHistory::EndChange(const std::string& name, const std::vector<Scene::Entity>& roots) {

        if (!changeStarted)
            return;
        changeStarted = false;

        auto before = std::move(hierarchiesBefore);
        auto after = SaveHierarchies(roots);
        if (before.empty() && after.empty())
            return;

        history.Push({
            .name = name,
            .undo = [this, before, after]() {
                RemoveHierarchies(after);
                RestoreHierarchies(before);
            },
            .redo = [this, before, after]() {
                RemoveHierarchies(before);
                RestoreHierarchies(after);
            }
        });

        // Edits of the changed entities start from their new state
        ResetStates();

    }

    void SceneHistory::BeginMove(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities) {

        this->scene = scene;

        std::vector<ECS::Entity> ecsEntities(entities.begin(), entities.end());
        placementsBefore = SavePlacements(ecsEntities);

    }

    void SceneHistory::EndMove(const std::string& name) {

        auto before = std::move(placementsBefore);
        std::vector<ECS::Entity> entities;
        for (const auto& placement : before)
            entities.push_back(placement.entity);
        auto after = SavePlacements(entities);

        auto moved = false;
        for (size_t i = 0; i < before.size(); i++)
            moved |= before[i].parent != after[i].parent || before[i].index != after[i].index;
        if (!moved)
            return;

        history.Push({
            .name = name,
            .undo = [this, before]() { RestorePlacements(before); },
            .redo = [this, after]() { RestorePlacements(after); }
        });

        ResetStates();

    }

    void SceneHistory::BeginEdit(const Ref<Scene::Scene>& scene, const std::vector<Scene::Entity>& entities) {

        this->scene = scene;

        editedEntities.clear();
        for (auto entity : entities)
            if (entity.IsValid())
                editedEntities.push_back({ entity, SaveComponents(entity) });

    }

    void SceneHistory::EndEdit(const std::string& name) {

        std::vector<EntityChange> changes;
        for (auto& change : editedEntities) {
            Scene::Entity entity(change.entity, &scene->entityManager);
            if (!entity.IsValid())
                continue;
            change.after = SaveComponents(entity);
            if (!IsSameEntity(change.before, change.after))
                changes.push_back(change);
        }
        editedEntities.clear();

        if (changes.empty())
            return;

        PushEntityChanges(name, changes);
        ResetStates();

    }

    bool SceneHistory::Undo(const Ref<Scene::Scene>& scene) {

        this->scene = scene;

        changedEntities.clear();
        auto undone = history.Undo();
        ResetStates();
        return undone;

    }

    bool SceneHistory::Redo(const Ref<Scene::Scene>& scene) {

        this->scene = scene;

        changedEntities.clear();
        auto redone = history.Redo();
        ResetStates();
        return redone;

    }

    void SceneHistory::Clear() {

        history.Clear();
        ResetStates();
        remappedEntities.clear();
        changedEntities.clear();

    }

    std::vector<Scene::Entity> SceneHistory::GetChangedEntities() {

        std::vector<Scene::Entity> entities;
        for (auto ecsEntity : changedEntities) {
            Scene::Entity entity(Resolve(ecsEntity), &scene->entityManager);
            if (entity.IsValid() && std::find(entities.begin(), entities.end(), entity) == entities.end())
                entities.push_back(entity);
        }
        return entities;

    }

    void SceneHistory::PushEntityChanges(const std::string& name, const std::vector<EntityChange>& changes) {

        auto restore = [this, changes](bool undo) {
            for (const auto& change : changes) {
                Scene::Entity entity(Resolve(change.entity), &scene->entityManager);
                if (!entity.IsValid())
                    continue;
                RestoreComponents(entity, undo ? change.before : change.after);
                changedEntities.push_back(entity);
            }
            };

        history.Push({
            .name = name,
            .undo = [restore]() { restore(true); },
            .redo = [restore]() { restore(false); }
        });

    }

    std::vector<SceneHistory::Hierarchy> SceneHistory::SaveHierarchies(const std::vector<Scene::Entity>& roots) {

        std::vector<Hierarchy> hierarchies;
        for (auto root : roots) {
            if (!root.IsValid())
                continue;

            // Descendants of other roots are part of their hierarchy already
            auto hasRootAncestor = false;
            for (auto parent = scene->GetParentEntity(root); parent.IsValid(); parent = scene->GetParentEntity(parent))
                hasRootAncestor |= std::find(roots.begin(), roots.end(), parent) != roots.end();
            if (hasRootAncestor)
                continue;

            auto& hierarchy = hierarchies.emplace_back();
            auto parent = scene->GetParentEntity(root);
            hierarchy.parent = parent;
            hierarchy.index = 0;
            if (parent.IsValid()) {
                const auto& children = parent.GetComponent<HierarchyComponent>().GetChildren();
                hierarchy.index = size_t(std::find(children.begin(), children.end(), root) - children.begin());
            }

            std::set<ECS::Entity> insertedEntities;
            Scene::EntityToJson(hierarchy.entity, root, scene.get(), insertedEntities);
        }
        return hierarchies;

    }

    void SceneHistory::RemoveHierarchies(const std::vector<Hierarchy>& hierarchies) {

        for (const auto& hierarchy : hierarchies) {
            Scene::Entity entity(Resolve(hierarchy.entity["id"]), &scene->entityManager);
            if (entity.IsValid())
                scene->DestroyEntity(entity);
        }

    }

    void SceneHistory::RestoreHierarchies(std::vector<Hierarchy> hierarchies) {

        // Siblings are inserted from the front, such that each one ends up at its index again
        std::sort(hierarchies.begin(), hierarchies.end(),
            [](const Hierarchy& a, const Hierarchy& b) { return a.index < b.index; });

        for (const auto& hierarchy : hierarchies) {
            auto entity = RestoreEntity(hierarchy.entity);
            changedEntities.push_back(entity);

            PlaceEntity(entity, hierarchy.parent, hierarchy.index);
        }

    }

    std::vector<SceneHistory::Placement> SceneHistory::SavePlacements(const std::vector<ECS::Entity>& entities) {

        std::vector<Placement> placements;
        for (auto ecsEntity : entities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            auto parent = scene->GetParentEntity(entity);
            size_t index = 0;
            if (parent.IsValid()) {
                const auto& children = parent.GetComponent<HierarchyComponent>().GetChildren();
                index = size_t(std::find(children.begin(), children.end(), entity) - children.begin());
            }
            placements.push_back({ ecsEntity, parent, index });
        }
        return placements;

    }

    void SceneHistory::RestorePlacements(std::vector<Placement> placements) {

        // Siblings are inserted from the front, such that each one ends up at its index again
        std::sort(placements.begin(), placements.end(),
            [](const Placement& a, const Placement& b) { return a.index < b.index; });

        for (const auto& placement : placements) {
            Scene::Entity entity(Resolve(placement.entity), &scene->entityManager);
            if (!entity.IsValid())
                continue;

            PlaceEntity(entity, placement.parent, placement.index);
            changedEntities.push_back(entity);
        }

    }

    void SceneHistory::PlaceEntity(Scene::Entity entity, ECS::Entity parent, size_t index) {

        Scene::Entity parentEntity(Resolve(parent), &scene->entityManager);
        if (!parentEntity.IsValid())
            return;

        entity.SetParent(parentEntity);
        auto& children = parentEntity.GetComponent<HierarchyComponent>().GetChildren();
        auto iter = std::find(children.begin(), children.end(), entity);
        children.erase(iter);
        children.insert(children.begin() + std::min(index, children.size()), entity);

    }

    Scene::Entity SceneHistory::RestoreEntity(const json& j) {

        auto id = Resolve(j["id"]);
        auto ecsEntity = scene->entityManager.Create(id);
        if (ecsEntity == ECS::EntityConfig::InvalidEntity) {
            ecsEntity = scene->entityManager.Create();
            remappedEntities[id] = ecsEntity;
        }

        Scene::Entity entity(ecsEntity, &scene->entityManager);

        auto components = j;
        components.erase("id");
        components.erase("entities");
        Scene::EntityFromJson(components, entity, scene.get(), false);

        // Children need their components before the parent adds its hierarchy, see EntityFromJson
        if (j.contains("entities")) {
            std::vector<Scene::Entity> children;
            for (const auto& child : j["entities"])
                children.push_back(RestoreEntity(child));

            auto& hierarchy = entity.AddComponent<HierarchyComponent>();
            hierarchy.root = j["root"];
            for (auto child : children)
                hierarchy.AddChild(child);
        }

        return entity;

    }

    json SceneHistory::SaveComponents(Scene::Entity entity) {

        json j;
        Scene::ComponentsToJson(j, entity, scene.get());
        if (auto comp = entity.TryGetComponent<HierarchyComponent>())
            j["root"] = comp->root;

        return j;

    }

    void SceneHistory::RestoreComponents(Scene::Entity entity, const json& j) {

        auto current = SaveComponents(entity);

        for (const auto& [key, value] : current.items()) {
            if (key == "root" || (j.contains(key) && IsSameComponent(key, j[key], value)))
                continue;

            // The transform is set in place, removing it would detach e.g. the rigid body
            if (key == "transform" && j.contains(key)) {
                TransformComponent transform = j[key];
                auto& comp = entity.GetComponent<TransformComponent>();
                comp.isStatic = transform.isStatic;
                comp.parentJoint = transform.parentJoint;
                comp.Set(transform.matrix);
                continue;
            }

            RemoveComponent(entity, key);
        }

        json added = json::object();
        for (const auto& [key, value] : j.items()) {
            if (key == "root" || (key == "transform" && current.contains(key)))
                continue;
            if (!current.contains(key) || !IsSameComponent(key, current[key], value))
                added[key] = value;
        }
        Scene::EntityFromJson(added, entity, scene.get(), false);

        auto hierarchy = entity.TryGetComponent<HierarchyComponent>();
        if (hierarchy && j.contains("root"))
            hierarchy->root = j["root"];

    }

    void SceneHistory::RemoveComponent(Scene::Entity entity, const std::string& key) {

        if (key == "name")
            entity.RemoveComponent<NameComponent>();
        else if (key == "transform")
            entity.RemoveComponent<TransformComponent>();
        else if (key == "mesh")
            entity.RemoveComponent<MeshComponent>();
        else if (key == "light")
            entity.RemoveComponent<LightComponent>();
        else if (key == "camera")
            entity.RemoveComponent<CameraComponent>();
        else if (key == "audio")
            entity.RemoveComponent<AudioComponent>();
        else if (key == "audioVolume")
            entity.RemoveComponent<AudioVolumeComponent>();
        else if (key == "fogVolume")
            entity.RemoveComponent<FogVolumeComponent>();
        else if (key == "rigidBody")
            entity.RemoveComponent<RigidBodyComponent>();
        else if (key == "player")
            entity.RemoveComponent<PlayerComponent>();
        else if (key == "constraint")
            entity.RemoveComponent<ConstraintComponent>();
        else if (key == "text")
            entity.RemoveComponent<TextComponent>();
        else if (key == "script")
            entity.RemoveComponent<LuaScriptComponent>();
        else if (key == "spline")
            entity.RemoveComponent<SplineComponent>();
        else if (key == "animation")
            entity.RemoveComponent<AnimationComponent>();

    }

    bool SceneHistory::IsSameEntity(const json& a, const json& b) {

        if (a.size() != b.size())
            return false;

        for (const auto& [key, value] : a.items())
            if (!b.contains(key) || !IsSameComponent(key, value, b[key]))
                return false;

        return true;

    }

    bool SceneHistory::IsSameComponent(const std::string& key, const json& a, const json& b) {

        // The transform panel rebuilds the local matrix from the global one, which changes it by rounding errors
        if (key == "transform") {
            TransformComponent transformA = a, transformB = b;
            if (transformA.isStatic != transformB.isStatic || transformA.parentJoint != transformB.parentJoint)
                return false;
            for (int32_t i = 0; i < 4; i++) {
                auto tolerance = 1e-5f * glm::max(vec4(1.0f), glm::abs(transformA.matrix[i]));
                if (glm::any(glm::greaterThan(glm::abs(transformA.matrix[i] - transformB.matrix[i]), tolerance)))
                    return false;
            }
            return true;
        }

        if (key != "animation" && key != "audio" && key != "rigidBody" && key != "light" && key != "camera")
            return a == b;

        // Runtime state which isn't edited, e.g. the shadow cascades follow the camera
        auto withoutRuntimeState = [&](json j) {
            if (key == "animation") {
                j.erase("time");
                if (j.contains("layers"))
                    for (auto& layer : j["layers"])
                        layer.erase("time");
            }
            if (key == "audio" && j.contains("stream"))
                j["stream"].erase("time");
            if (key == "rigidBody")
                j.erase("bodyIndex");
            if (key == "light" && j.contains("shadow")) {
                j["shadow"].erase("views");
                j["shadow"].erase("center");
            }
            if (key == "camera")
                j.erase("aspectRatio");
            return j;
            };
        return withoutRuntimeState(a) == withoutRuntimeState(b);

    }

    json SceneHistory::SaveSettings() {

        json j;
        Scene::SceneSettingsToJson(j, scene.get());
        return j;

    }

    bool SceneHistory::IsSameSettings(const json& a, const json& b) {

        if (a.size() != b.size())
            return false;

        for (const auto& [key, value] : a.items())
            if (!b.contains(key) || !IsSameSetting(key, value, b[key]))
                return false;

        return true;

    }

    bool SceneHistory::IsSameSetting(const std::string& key, const json& a, const json& b) {

        if (key != "irradianceVolume")
            return a == b;

        // A scrolling volume follows the camera, only its size is a setting
        auto withoutPosition = [](json j) {
            if (!j.value("scroll", false) || !j.contains("aabb"))
                return j;
            const auto& aabb = j["aabb"];
            auto size = json::array();
            for (const auto* axis : { "x", "y", "z" })
                size.push_back(std::round((aabb["max"][axis].get<float>() - aabb["min"][axis].get<float>()) * 100.0f));
            j.erase("aabb");
            j["size"] = size;
            return j;
            };
        return withoutPosition(a) == withoutPosition(b);

    }

    void SceneHistory::RestoreSettings(const json& j) {

        // Only changed settings are restored, e.g. the irradiance volume would need to be rebuilt
        auto current = SaveSettings();
        json changed = json::object();
        for (const auto& [key, value] : j.items())
            if (!current.contains(key) || !IsSameSetting(key, current[key], value))
                changed[key] = value;

        Scene::SceneSettingsFromJson(changed, scene.get());

        // Features which didn't exist in the restored state are removed again
        if (!j.contains("fog"))
            scene->fog = nullptr;
        if (!j.contains("irradianceVolume"))
            scene->irradianceVolume = nullptr;
        if (!j.contains("reflection"))
            scene->reflection = nullptr;
        if (!j.contains("rtgi"))
            scene->rtgi = nullptr;
        if (!j.contains("sss"))
            scene->sss = nullptr;
        if (!j.contains("ssgi"))
            scene->ssgi = nullptr;
        if (!j.contains("ocean"))
            scene->ocean = nullptr;
        if (!j.contains("navigation"))
            scene->navigation = nullptr;

    }

    void SceneHistory::ResetStates() {

        entityStates.clear();
        settingsState = json();

    }

    ECS::Entity SceneHistory::Resolve(ECS::Entity entity) {

        // Each restore with a new id adds a link, a chain can't be longer than the history
        for (size_t i = 0; i <= history.maxSteps; i++) {
            auto iter = remappedEntities.find(entity);
            if (iter == remappedEntities.end())
                break;
            entity = iter->second;
        }
        return entity;

    }

}
