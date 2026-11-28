#include "WindowShot.h"

#include "graphics/GraphicsDevice.h"
#include "loader/ImageLoader.h"
#include "common/Image.h"

namespace Atlas::Bridge {

    bool WindowShot::Copy(const std::string& path) {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto swapChain = device->swapChain;
        auto format = swapChain->surfaceFormat.format;
        bgra = format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
        auto rgba = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
        if (!swapChain->isComplete || !swapChain->readable || !(bgra || rgba))
            return false;

        extent = swapChain->extent;
        Graphics::BufferDesc desc = {
            .usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .domain = Graphics::BufferDomain::Host,
            .hostAccess = Graphics::BufferHostAccess::Random,
            .size = size_t(extent.width) * size_t(extent.height) * 4,
        };
        buffer = device->CreateBuffer(desc);
        this->path = path;

        auto image = swapChain->images[swapChain->aquiredImageIndex];
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        auto commandList = device->GetCommandList();
        commandList->BeginCommands();
        vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy = {};
        copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        copy.imageExtent = { extent.width, extent.height, 1 };
        vkCmdCopyImageToBuffer(commandList->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer->buffer, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        commandList->EndCommands();
        device->SubmitCommandList(commandList);

        return true;

    }

    bool WindowShot::Save() {

        if (!buffer)
            return false;

        // Waiting for the previous submission isn't enough, parts of the buffer still held the last picture
        Graphics::GraphicsDevice::DefaultDevice->WaitForIdle();

        auto width = int32_t(extent.width), height = int32_t(extent.height);
        auto image = CreateRef<Common::Image<uint8_t>>(width, height, 4);
        auto& pixels = image->GetData();
        auto source = static_cast<const uint8_t*>(buffer->Map());
        for (size_t i = 0; i < pixels.size(); i += 4) {
            pixels[i] = source[i + (bgra ? 2 : 0)];
            pixels[i + 1] = source[i + 1];
            pixels[i + 2] = source[i + (bgra ? 0 : 2)];
            pixels[i + 3] = 255;
        }
        buffer->Unmap();
        image->fileFormat = Common::ImageFormat::PNG;
        Loader::ImageLoader::SaveImage(image, path);
        buffer.reset();

        return true;

    }

}
