#include "PrimitiveBatchWrapper.h"

#include "graphics/GraphicsDevice.h"

namespace Atlas::Editor {

    PrimitiveBatchWrapper::PrimitiveBatchWrapper() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        primitiveBatchNoDepthTest = CreateRef<Renderer::PrimitiveBatch>();
        primitiveBatchNoDepthTest->SetLineWidth(device->support.wideLines ? 2.0f : 1.0f);
        primitiveBatchNoDepthTest->testDepth = false;

        primitiveBatchDepthTest = CreateRef<Renderer::PrimitiveBatch>();
        primitiveBatchDepthTest->SetLineWidth(device->support.wideLines ? 2.0f : 1.0f);
        primitiveBatchDepthTest->testDepth = true;

    }

    const Ref<Renderer::PrimitiveBatch>& PrimitiveBatchWrapper::GetBatch(bool depthTest) const {

        return depthTest ? primitiveBatchDepthTest : primitiveBatchNoDepthTest;

    }

    void PrimitiveBatchWrapper::Clear() {

        primitiveBatchDepthTest->Clear();
        primitiveBatchNoDepthTest->Clear();

    }

}