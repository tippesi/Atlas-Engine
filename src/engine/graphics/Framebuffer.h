#ifndef AE_GRAPHICSFRAMEBUFFER_H
#define AE_GRAPHICSFRAMEBUFFER_H

#include "Common.h"
#include "RenderPass.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        enum class Attachment {
            ColorAttachment0 = 0,
            ColorAttachment1 = 1,
            ColorAttachment2 = 2,
            ColorAttachment3 = 3,
            ColorAttachment4 = 4,
            ColorAttachment5 = 5,
            ColorAttachment6 = 6,
            ColorAttachment7 = 7,
            DepthAttachment
        };

        struct FrameBufferAttachmentDesc {
            Attachment attachment;
            uint32_t layer = 0;
        };

        struct FrameBufferDesc {
            Ref<RenderPass> renderPass;
            std::vector<FrameBufferAttachmentDesc> attachments;

            VkExtent2D extent;
            uint32_t layers = 1;
        };

        struct FrameBufferAttachment {
            Attachment attachment;

            uint32_t idx = 0;
            uint32_t layer = 0;

            bool isValid = false;
        };

        class FrameBuffer {
        public:
            FrameBuffer(GraphicsDevice* device, FrameBufferDesc& desc);

            ~FrameBuffer();

            void Refresh();

            VkFramebuffer frameBuffer;

            Ref<RenderPass> renderPass;

            FrameBufferAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            FrameBufferAttachment depthAttachment;

            VkExtent2D extent;
            uint32_t layers;

            bool isComplete = false;

        private:
            GraphicsDevice* device;

        };

    }

}


#endif
