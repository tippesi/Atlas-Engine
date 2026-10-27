#include "Entity.h"
#include "Scene.h"
#include "SceneSerializer.h"

namespace Atlas::Scene {

	void Entity::MarkDirty() const {

		AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");
		GetScene()->MarkEntityDirty(*this);

	}

	std::vector<uint8_t> Entity::Backup(const Ref<Scene>& scene, const Entity& entity) {

		json j;
		std::set<ECS::Entity> insertedEntities;
		EntityToJson(j, entity, scene.get(), insertedEntities);

		return json::to_msgpack(j);

	}

	void Entity::SetParent(Entity parent) {

		AE_ASSERT(IsValid() && "Entity is not valid and doesn't belong to any scene");
		AE_ASSERT(entity != parent.entity && "Entity can't be its own parent");

		auto scene = GetScene();
		auto currentParent = scene->GetParentEntity(*this);
		if (currentParent.entity == parent.entity)
			return;

		if (currentParent.IsValid())
			currentParent.GetComponent<Components::HierarchyComponent>().RemoveChild(*this);

		if (!parent.IsValid())
			return;

		if (!parent.HasComponent<Components::HierarchyComponent>())
			parent.AddComponent<Components::HierarchyComponent>();
		parent.GetComponent<Components::HierarchyComponent>().AddChild(*this);

	}

	Entity Entity::Restore(const Ref<Scene>& scene, const std::vector<uint8_t>& serialized) {

		json j = json::from_msgpack(serialized);

		auto entity = scene->CreateEntity();

		EntityFromJson(j, entity, scene.get(), false);

		return entity;

	}

}