#include "PrimitiveBatchWrapper.h"

#include "graphics/GraphicsDevice.h"

namespace Atlas::Editor {

    PrimitiveBatchWrapper::PrimitiveBatchWrapper() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        primitiveBatch = CreateRef<Renderer::PrimitiveBatch>();
        primitiveBatch->SetLineWidth(device->support.wideLines ? 2.0f : 1.0f);

    }

    void PrimitiveBatchWrapper::RenderLineAABB(Volume::AABB aabb, glm::vec3 color) {

        auto corners = aabb.GetCorners();

        primitiveBatch->AddLine(corners[0], corners[1], color, color);
        primitiveBatch->AddLine(corners[1], corners[2], color, color);
        primitiveBatch->AddLine(corners[2], corners[3], color, color);
        primitiveBatch->AddLine(corners[3], corners[0], color, color);
        primitiveBatch->AddLine(corners[4], corners[5], color, color);
        primitiveBatch->AddLine(corners[5], corners[6], color, color);
        primitiveBatch->AddLine(corners[6], corners[7], color, color);
        primitiveBatch->AddLine(corners[7], corners[4], color, color);
        primitiveBatch->AddLine(corners[0], corners[4], color, color);
        primitiveBatch->AddLine(corners[1], corners[5], color, color);
        primitiveBatch->AddLine(corners[2], corners[6], color, color);
        primitiveBatch->AddLine(corners[3], corners[7], color, color);

    }

    void PrimitiveBatchWrapper::RenderLineFrustum(Volume::Frustum frustum, glm::vec3 color) {

        auto corners = frustum.GetCorners();
        if (corners.empty())
            return;

        primitiveBatch->AddLine(corners[0], corners[1], color, color);
        primitiveBatch->AddLine(corners[2], corners[3], color, color);
        primitiveBatch->AddLine(corners[0], corners[2], color, color);
        primitiveBatch->AddLine(corners[1], corners[3], color, color);
        primitiveBatch->AddLine(corners[4], corners[5], color, color);
        primitiveBatch->AddLine(corners[6], corners[7], color, color);
        primitiveBatch->AddLine(corners[4], corners[6], color, color);
        primitiveBatch->AddLine(corners[5], corners[7], color, color);
        primitiveBatch->AddLine(corners[0], corners[4], color, color);
        primitiveBatch->AddLine(corners[1], corners[5], color, color);
        primitiveBatch->AddLine(corners[2], corners[6], color, color);
        primitiveBatch->AddLine(corners[3], corners[7], color, color);

    }

    void PrimitiveBatchWrapper::RenderLineSphere(vec3 position, float radius, vec3 color) {

        const int32_t verticalSubdivs = 20;
        const int32_t horizontalSubdivs = 20;

        vec3 lastPoint;

        float alpha = 0.0f;

        for (int32_t i = 0; i < horizontalSubdivs; i++) {

            float ringRadius = sinf(alpha);
            float ringHeight = cosf(alpha);
            float beta = 0.0f;

            for (int32_t j = 0; j < verticalSubdivs; j++) {

                float x = sinf(beta) * ringRadius;
                float z = cosf(beta) * ringRadius;
                auto point = vec3(x, ringHeight, z) * radius + position;

                if (j > 0) {
                    primitiveBatch->AddLine(point, lastPoint, color, color);
                }

                beta += 2.0f * glm::pi<float>() / float(verticalSubdivs - 1);

                lastPoint = point;

            }

            alpha += glm::pi<float>() / float(horizontalSubdivs);

        } 

    }

    void PrimitiveBatchWrapper::RenderLineRectangle(const Volume::Rectangle& rect, vec3 color) {

        auto point = rect.point;
        auto right = rect.s0;
        auto down = rect.s1;

        primitiveBatch->AddLine(point, point + right, color, color);
        primitiveBatch->AddLine(point, point + down, color, color);
        primitiveBatch->AddLine(point + right, point + right + down, color, color);
        primitiveBatch->AddLine(point + down, point + right + down, color, color);

    }

}