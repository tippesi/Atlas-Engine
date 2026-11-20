#include "Exposure.h"
#include "../graphics/GraphicsDevice.h"

namespace Atlas::PostProcessing {

    AutoExposure::AutoExposure() : histogramBuffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t), 256) {

        std::vector<uint32_t> histogramInitialData(histogramBuffer.GetElementCount(), 0);
        histogramBuffer.SetData(histogramInitialData.data(), 0, histogramInitialData.size());

        float noLuminance = 0.0f;
        Graphics::BufferDesc desc = {
            .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .domain = Graphics::BufferDomain::Host,
            .hostAccess = Graphics::BufferHostAccess::Random,
            .data = &noLuminance,
            .size = sizeof(float)
        };
        luminanceReadback = Graphics::GraphicsDevice::DefaultDevice->CreateMultiBuffer(desc);

    }

    float AutoExposure::GetMeasuredLuminance() const {

        // The copy of the current frame index was written by a frame which completed already
        luminanceReadback->Invalidate();
        auto luminance = *static_cast<float*>(luminanceReadback->Map());
        luminanceReadback->Unmap();
        return luminance;

    }

}