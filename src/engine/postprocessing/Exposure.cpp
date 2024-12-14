#include "Exposure.h"

namespace Atlas::PostProcessing {

    Exposure::Exposure() : histogramBuffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t), 256) {



    }

}