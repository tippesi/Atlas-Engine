#pragma once

#include "Common.h"
#include "RenderPass.h"

namespace Atlas {

    namespace Graphics {

        class GraphicsDevice;

        struct FrameBufferAttachmentDesc {
            Ref<Image> image = nullptr;

            uint32_t layer = 0;
            bool writeEnabled = true;
        };

        struct FrameBufferDesc {
            Ref<RenderPass> renderPass;

            FrameBufferAttachmentDesc colorAttachments[MAX_COLOR_ATTACHMENTS];
            FrameBufferAttachmentDesc depthAttachment;

            VkExtent2D extent;
        };

        struct FrameBufferAttachment {
            Ref<Image> image = nullptr;
            uint32_t layer = 0;

            bool isValid = false;
            bool writeEnabled = true;
        };

        // The images a render pass renders into, their views are taken when the pass begins
        class FrameBuffer {
        public:
            FrameBuffer(GraphicsDevice* device, const FrameBufferDesc& desc);

            void ChangeColorAttachmentImage(const Ref<Image>& image, const uint32_t slot);

            void ChangeColorAttachmentImage(const Ref<Image>& image, const uint32_t layer, const uint32_t slot);

            void ChangeDepthAttachmentImage(const Ref<Image>& image);

            Ref<Image>& GetColorImage(uint32_t slot);

            Ref<Image>& GetDepthImage();

            Ref<RenderPass> renderPass;

            FrameBufferAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
            FrameBufferAttachment depthAttachment;

            VkExtent2D extent;

        };

    }

}