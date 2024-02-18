#include "TextComponent.h"

namespace Atlas::Scene::Components {

    void TextComponent::ChangeResource(const ResourceHandle<Font>& font) {

        this->font = font;

    }

    Volume::Rectangle TextComponent::GetRectangle() const {

        auto right = glm::mat3_cast(transformedRotation) * glm::vec3(1.0f, 0.0f, 0.0f);
        auto down = glm::mat3_cast(transformedRotation) * glm::vec3(0.0f, -1.0f, 0.0f);
            
        right *= 2.0f * halfSize.x;
        down *= 2.0f * halfSize.y;

        auto rectOrigin = transformedPosition - right * 0.5f - down * 0.5f;

        return Volume::Rectangle(rectOrigin, right, down);

    }

    void TextComponent::Update(const TransformComponent& transform) {
    
        transformedPosition = vec4(transform.globalMatrix * vec4(position, 1.0f));
        transformedRotation = glm::quat_cast(transform.globalMatrix) * rotation;
    
    }

}