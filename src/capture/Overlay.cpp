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

    std::span<Ref<Renderer::PrimitiveBatch>> Overlay::GetBatches() {

        return batches;

    }

    Ref<Renderer::PrimitiveBatch>& Overlay::GetBatch() {

        return depthTest ? batches[0] : batches[1];

    }

}
