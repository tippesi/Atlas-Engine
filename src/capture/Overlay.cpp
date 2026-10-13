#include "Overlay.h"

#include "graphics/GraphicsDevice.h"

namespace Atlas::Capture {

    Overlay::Overlay() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        for (size_t i = 0; i < batches.size(); i++) {
            batches[i] = CreateRef<Renderer::PrimitiveBatch>();
            batches[i]->testDepth = i == 0;
            batches[i]->SetLineWidth(device->support.wideLines ? 2.0f : 1.0f);
        }

    }

    void Overlay::Clear() {

        for (auto& batch : batches)
            batch->Clear();

    }

    void Overlay::Line(vec3 from, vec3 to, vec3 color) {

        GetBatch()->AddLine(from, to, color, color);

    }

    void Overlay::Box(Volume::AABB aabb, vec3 color) {

        auto corners = aabb.GetCorners();

        for (int32_t i = 0; i < 4; i++) {
            Line(corners[i], corners[(i + 1) % 4], color);
            Line(corners[i + 4], corners[(i + 1) % 4 + 4], color);
            Line(corners[i], corners[i + 4], color);
        }

    }

    void Overlay::Cross(vec3 position, float size, vec3 color) {

        Line(position - vec3(size, 0.0f, 0.0f), position + vec3(size, 0.0f, 0.0f), color);
        Line(position - vec3(0.0f, size, 0.0f), position + vec3(0.0f, size, 0.0f), color);
        Line(position - vec3(0.0f, 0.0f, size), position + vec3(0.0f, 0.0f, size), color);

    }

    void Overlay::Figure(vec3 feet, float height, float radius, vec3 color) {

        const int32_t segments = 12;
        for (auto y : { 0.0f, height * 0.5f, height }) {
            for (int32_t i = 0; i < segments; i++) {
                auto a0 = float(i) / float(segments) * glm::two_pi<float>();
                auto a1 = float(i + 1) / float(segments) * glm::two_pi<float>();
                Line(feet + vec3(std::cos(a0) * radius, y, std::sin(a0) * radius),
                    feet + vec3(std::cos(a1) * radius, y, std::sin(a1) * radius), color);
            }
        }

        for (int32_t i = 0; i < 4; i++) {
            auto angle = float(i) * glm::half_pi<float>();
            auto offset = vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
            Line(feet + offset, feet + offset + vec3(0.0f, height, 0.0f), color);
        }

    }

    void Overlay::Axes(vec3 position, float length) {

        Line(position, position + vec3(length, 0.0f, 0.0f), vec3(1.0f, 0.1f, 0.1f));
        Line(position, position + vec3(0.0f, length, 0.0f), vec3(0.1f, 1.0f, 0.1f));
        Line(position, position + vec3(0.0f, 0.0f, length), vec3(0.2f, 0.4f, 1.0f));

    }

    std::span<Ref<Renderer::PrimitiveBatch>> Overlay::GetBatches() {

        return batches;

    }

    Ref<Renderer::PrimitiveBatch>& Overlay::GetBatch() {

        return depthTest ? batches[0] : batches[1];

    }

}
