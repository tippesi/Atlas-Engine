#include "TextComponent.h"

namespace Atlas::Scene::Components {

    void TextComponent::ChangeResource(const ResourceHandle<Font>& font) {

        this->font = font;

    }

    void TextComponent::Update(const TransformComponent& transform) {
    
        transformedPosition = vec4(transform.globalMatrix * vec4(position, 1.0f));
        transformedRotation = glm::quat_cast(transform.globalMatrix) * rotation;
    
    }

}