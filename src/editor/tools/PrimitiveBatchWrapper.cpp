#include "PrimitiveBatchWrapper.h"

namespace Atlas::Editor {

    PrimitiveBatchWrapper::PrimitiveBatchWrapper() {

        primitiveBatchNoDepthTest = CreateRef<Renderer::PrimitiveBatch>();
        primitiveBatchNoDepthTest->testDepth = false;

        primitiveBatchDepthTest = CreateRef<Renderer::PrimitiveBatch>();
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