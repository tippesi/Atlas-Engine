#pragma once

#include "renderer/PrimitiveBatch.h"

namespace Atlas::Editor {

    // Debug lines of the viewport, drawn with and without depth test
    class PrimitiveBatchWrapper {

    public:
        PrimitiveBatchWrapper();

        const Ref<Renderer::PrimitiveBatch>& GetBatch(bool depthTest) const;

        void Clear();

        Ref<Renderer::PrimitiveBatch> primitiveBatchNoDepthTest;
        Ref<Renderer::PrimitiveBatch> primitiveBatchDepthTest;

    };

}