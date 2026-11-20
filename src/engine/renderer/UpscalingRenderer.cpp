#include "UpscalingRenderer.h"
#include "helper/HaltonSequence.h"

namespace Atlas::Renderer {

    vec2 UpscalingRenderer::GetJitter(const Ref<RenderTarget>& target, uint32_t frameIndex) const {

        auto ratio = float(target->GetWidth()) / float(target->GetScaledWidth());
        auto phaseCount = std::max(uint32_t(8.0f * ratio * ratio), 8u);
        auto index = int32_t(frameIndex % phaseCount + 1);

        auto jitter = vec2(Helper::HaltonSequence::Halton(2, index), Helper::HaltonSequence::Halton(3, index)) - 0.5f;
        return 2.0f * jitter / vec2(float(target->GetScaledWidth()), float(target->GetScaledHeight()));

    }

    bool VendorUpscalingRenderer::Prepare(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) {

        auto renderSize = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
        auto displaySize = ivec2(target->GetWidth(), target->GetHeight());

        bool failed = renderSize == failedRenderSize && displaySize == failedDisplaySize;
        if (HasContext() && !failed && renderSize == contextRenderSize && displaySize == contextDisplaySize)
            return true;

        DestroyContext();
        if (failed)
            return false;

        contextRenderSize = renderSize;
        contextDisplaySize = displaySize;

        if (!CreateContext(renderSize, displaySize, commandList)) {
            MarkContextFailed();
            return false;
        }

        return true;

    }

    void VendorUpscalingRenderer::MarkContextFailed() {

        failedRenderSize = contextRenderSize;
        failedDisplaySize = contextDisplaySize;

    }

}
