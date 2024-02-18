#pragma once

#include "../../System.h"

#include "TransformComponent.h"

#include "../../Font.h"
#include "../../volume/Rectangle.h"
#include "../../resource/Resource.h"

namespace Atlas::Scene {

    class Scene;

	namespace Components {

        class HierarchyComponent;

        class TextComponent {

            friend Scene;

        public:
            TextComponent() = default;
            TextComponent(const TextComponent& that) = default;
            explicit TextComponent(const ResourceHandle<Font>& font, const std::string& text) : font(font), text(text) {}

            void ChangeResource(const ResourceHandle<Font>& font);

            Volume::Rectangle GetRectangle() const;

            vec3 GetTransformedPosition() const { return transformedPosition; }
            quat GetTransformedRotation() const { return transformedRotation; }

            ResourceHandle<Font> font;

            std::string text;

            vec3 position = vec3(0.0f);
            quat rotation = quat();

            vec2 halfSize = vec2(1.0f);

            vec4 textColor = vec4(1.0f);
            vec4 outlineColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);

            float outlineFactor = 0.0f;

            float textScale = 1.0f;

        protected:
            void Update(const TransformComponent& transform);

            vec3 transformedPosition;
            quat transformedRotation;

        };

	}

}