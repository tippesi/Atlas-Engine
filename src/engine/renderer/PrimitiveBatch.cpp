#include "PrimitiveBatch.h"
#include "../common/MathHelper.h"

#include <glm/gtc/constants.hpp>

namespace Atlas {

    namespace Renderer {

        PrimitiveBatch::PrimitiveBatch() {

            lineVertices = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);
            lineColors = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);

            triangleVertices = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);
            triangleColors = Buffer::VertexBuffer(VK_FORMAT_R32G32B32_SFLOAT, 0, nullptr, true);

        }

        void PrimitiveBatch::AddLine(vec3 from, vec3 to, vec3 fromColor, vec3 toColor) {

            lineVertexData.push_back(from);
            lineVertexData.push_back(to);

            lineColorData.push_back(fromColor);
            lineColorData.push_back(toColor);

            lineDataValid = false;

        }

        void PrimitiveBatch::AddLineAABB(Volume::AABB aabb, vec3 color) {

            auto corners = aabb.GetCorners();
            for (int32_t i = 0; i < 4; i++) {
                AddLine(corners[i], corners[(i + 1) % 4], color, color);
                AddLine(corners[i + 4], corners[(i + 1) % 4 + 4], color, color);
                AddLine(corners[i], corners[i + 4], color, color);
            }

        }

        void PrimitiveBatch::AddLineFrustum(const Volume::Frustum& frustum, vec3 color) {

            auto corners = frustum.GetCorners();
            for (int32_t i = 0; i < 2; i++) {
                auto offset = i * 4;
                AddLine(corners[offset], corners[offset + 1], color, color);
                AddLine(corners[offset + 2], corners[offset + 3], color, color);
                AddLine(corners[offset], corners[offset + 2], color, color);
                AddLine(corners[offset + 1], corners[offset + 3], color, color);
            }
            for (int32_t i = 0; i < 4; i++)
                AddLine(corners[i], corners[i + 4], color, color);

        }

        void PrimitiveBatch::AddLineSphere(vec3 center, float radius, vec3 color) {

            const int32_t verticalSubdivs = 20;
            const int32_t horizontalSubdivs = 20;

            float alpha = 0.0f;
            for (int32_t i = 0; i < horizontalSubdivs; i++) {
                float ringRadius = sinf(alpha);
                float ringHeight = cosf(alpha);
                float beta = 0.0f;

                vec3 lastPoint;
                for (int32_t j = 0; j < verticalSubdivs; j++) {
                    auto point = vec3(sinf(beta) * ringRadius, ringHeight, cosf(beta) * ringRadius) * radius + center;
                    if (j > 0)
                        AddLine(point, lastPoint, color, color);

                    beta += 2.0f * glm::pi<float>() / float(verticalSubdivs - 1);
                    lastPoint = point;
                }

                alpha += glm::pi<float>() / float(horizontalSubdivs);
            }

        }

        void PrimitiveBatch::AddLineSphereRings(vec3 center, float radius, vec3 color) {

            const int32_t segments = 48;
            for (int32_t i = 0; i < segments; i++) {
                auto a0 = float(i) / float(segments) * glm::two_pi<float>();
                auto a1 = float(i + 1) / float(segments) * glm::two_pi<float>();
                auto p0 = vec2(std::cos(a0), std::sin(a0)) * radius;
                auto p1 = vec2(std::cos(a1), std::sin(a1)) * radius;
                AddLine(center + vec3(p0.x, p0.y, 0.0f), center + vec3(p1.x, p1.y, 0.0f), color, color);
                AddLine(center + vec3(p0.x, 0.0f, p0.y), center + vec3(p1.x, 0.0f, p1.y), color, color);
                AddLine(center + vec3(0.0f, p0.x, p0.y), center + vec3(0.0f, p1.x, p1.y), color, color);
            }

        }

        void PrimitiveBatch::AddLineCircle(vec3 center, float radius, vec3 color) {

            const int32_t segments = 48;
            for (int32_t i = 0; i < segments; i++) {
                auto a0 = float(i) / float(segments) * glm::two_pi<float>();
                auto a1 = float(i + 1) / float(segments) * glm::two_pi<float>();
                AddLine(center + vec3(std::cos(a0), 0.0f, std::sin(a0)) * radius,
                    center + vec3(std::cos(a1), 0.0f, std::sin(a1)) * radius, color, color);
            }

        }

        void PrimitiveBatch::AddLineRectangle(const Volume::Rectangle& rectangle, vec3 color) {

            auto point = rectangle.point;
            auto right = rectangle.s0;
            auto down = rectangle.s1;

            AddLine(point, point + right, color, color);
            AddLine(point, point + down, color, color);
            AddLine(point + right, point + right + down, color, color);
            AddLine(point + down, point + right + down, color, color);

        }

        void PrimitiveBatch::AddLineCross(vec3 position, float size, vec3 color) {

            for (int32_t i = 0; i < 3; i++) {
                vec3 offset(0.0f);
                offset[i] = size;
                AddLine(position - offset, position + offset, color, color);
            }

        }

        void PrimitiveBatch::AddLineCylinder(vec3 base, float height, float radius, vec3 color) {

            const int32_t segments = 12;
            for (auto y : { 0.0f, height * 0.5f, height }) {
                for (int32_t i = 0; i < segments; i++) {
                    auto a0 = float(i) / float(segments) * glm::two_pi<float>();
                    auto a1 = float(i + 1) / float(segments) * glm::two_pi<float>();
                    AddLine(base + vec3(std::cos(a0) * radius, y, std::sin(a0) * radius),
                        base + vec3(std::cos(a1) * radius, y, std::sin(a1) * radius), color, color);
                }
            }

            for (int32_t i = 0; i < 4; i++) {
                auto angle = float(i) * glm::half_pi<float>();
                auto offset = vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
                AddLine(base + offset, base + offset + vec3(0.0f, height, 0.0f), color, color);
            }

        }

        void PrimitiveBatch::AddLineCone(vec3 apex, vec3 direction, float length, float angle, vec3 color) {

            auto right = Helper::Math::NormalizeOrZero(glm::cross(direction, std::abs(direction.y) < 0.99f ?
                vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)));
            auto up = glm::cross(right, direction);

            auto center = apex + direction * length * std::cos(angle);
            auto radius = length * std::sin(angle);

            const int32_t segments = 24;
            for (int32_t i = 0; i < segments; i++) {
                auto a0 = float(i) / float(segments) * glm::two_pi<float>();
                auto a1 = float(i + 1) / float(segments) * glm::two_pi<float>();
                auto p0 = center + (std::cos(a0) * right + std::sin(a0) * up) * radius;
                auto p1 = center + (std::cos(a1) * right + std::sin(a1) * up) * radius;
                AddLine(p0, p1, color, color);
                if (i % 6 == 0)
                    AddLine(apex, p0, color, color);
            }

        }

        void PrimitiveBatch::AddLineAxes(vec3 position, float length) {

            static const vec3 colors[] = { vec3(1.0f, 0.1f, 0.1f), vec3(0.1f, 1.0f, 0.1f), vec3(0.2f, 0.4f, 1.0f) };
            for (int32_t i = 0; i < 3; i++) {
                vec3 axis(0.0f);
                axis[i] = length;
                AddLine(position, position + axis, colors[i], colors[i]);
            }

        }

        void PrimitiveBatch::AddLineArrow(vec3 from, vec3 to, vec3 color) {

            AddLine(from, to, color, color);

            auto direction = to - from;
            auto length = glm::length(direction);
            if (length <= 0.0f)
                return;
            direction /= length;

            auto right = Helper::Math::NormalizeOrZero(glm::cross(direction, std::abs(direction.y) < 0.99f ?
                vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f)));
            auto up = glm::cross(right, direction);

            auto headLength = 0.2f * length;
            auto headBase = to - direction * headLength;
            for (auto side : { right, -right, up, -up })
                AddLine(to, headBase + side * 0.4f * headLength, color, color);

        }

        size_t PrimitiveBatch::GetLineCount() const {

            return lineVertexData.size() / 2;

        }

        void PrimitiveBatch::SetLineWidth(float width) {

            lineWidth = width;

        }

        float PrimitiveBatch::GetLineWidth() const {

            return lineWidth;

        }

        void PrimitiveBatch::AddTriangle(vec3 v0, vec3 v1, vec3 v2, vec3 v0Color,
            vec3 v1Color, vec3 v2Color) {

            triangleVertexData.push_back(v0);
            triangleVertexData.push_back(v1);
            triangleVertexData.push_back(v2);

            triangleColorData.push_back(v0Color);
            triangleColorData.push_back(v1Color);
            triangleColorData.push_back(v2Color);

            triangleDataValid = false;

        }

        size_t PrimitiveBatch::GetTriangleCount() const {

            return triangleVertexData.size() / 3;

        }

        void PrimitiveBatch::TransferData() {

            if (!lineDataValid && GetLineCount() > 0) {

                lineVertices.SetSize(GetLineCount() * 2,
                    lineVertexData.data());
                lineColors.SetSize(GetLineCount() * 2,
                    lineColorData.data());

                lineDataValid = true;

            }

            if (!triangleDataValid && GetTriangleCount() > 0) {

                triangleVertices.SetSize(GetTriangleCount() * 3,
                    triangleVertexData.data());
                triangleColors.SetSize(GetTriangleCount() * 3,
                    triangleColorData.data());

                triangleDataValid = true;

            }

            UpdateVertexArrays();

        }

        void PrimitiveBatch::Clear() {

            lineVertexData.clear();
            lineColorData.clear();

        }

        void PrimitiveBatch::UpdateVertexArrays() {

            if (lineVertices.elementCount)
                lineVertexArray.AddComponent(0, lineVertices);
            if (lineColors.elementCount)
                lineVertexArray.AddComponent(1, lineColors);

            if (triangleVertices.elementCount)
                triangleVertexArray.AddComponent(0, triangleVertices);
            if (triangleColors.elementCount)
                triangleVertexArray.AddComponent(1, triangleColors);

        }

    }

}