#include "Exposure.h"

namespace Atlas::PostProcessing {

    Exposure::Exposure() : histogramBuffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t), 256) {

        std::vector<uint32_t> histogramInitialData(histogramBuffer.GetElementCount(), 0);
        histogramBuffer.SetData(histogramInitialData.data(), 0, histogramInitialData.size());

    }

}