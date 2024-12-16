#include "Entity.h"
#include "Scene.h"
#include "SceneSerializer.h"

namespace Atlas::Scene {

	std::vector<uint8_t> Entity::Backup(const Ref<Scene>& scene, const Entity& entity) {

		json j;
		std::set<ECS::Entity> insertedEntities;
		EntityToJson(j, entity, scene.get(), insertedEntities);

		return json::to_msgpack(j);

	}

	Entity Entity::Restore(const Ref<Scene>& scene, const std::vector<uint8_t>& serialized) {

		json j = json::from_msgpack(serialized);

		auto entity = scene->CreateEntity();

		EntityFromJson(j, entity, scene.get(), false);

		return entity;

	}

}