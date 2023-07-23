#include "MemoryTransferManager.h"
#include "MemoryManager.h"
#include "GraphicsDevice.h"

#include "Buffer.h"
#include "Image.h"
#include "Format.h"

#include <cstring>
#include <cassert>

namespace Atlas {

    namespace Graphics {

        MemoryTransferManager::MemoryTransferManager(GraphicsDevice* device, MemoryManager *memManager)
            : device(device), memoryManager(memManager) {



        }

        MemoryTransferManager::~MemoryTransferManager() {

            

        }

        void MemoryTransferManager::UploadBufferData(void *data, Buffer* destinationBuffer,
            VkBufferCopy bufferCopyDesc) {

            auto commandList = device->GetCommandList(TransferQueue, true);

            VmaAllocator allocator = memoryManager->allocator;

            auto stagingAllocation = CreateStagingBuffer(bufferCopyDesc.size);

            void* destination;
            vmaMapMemory(allocator, stagingAllocation.allocation, &destination);
            std::memcpy(destination, data, bufferCopyDesc.size);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            commandList->BeginCommands();

            vkCmdCopyBuffer(commandList->commandBuffer, stagingAllocation.buffer,
                destinationBuffer->buffer, 1, &bufferCopyDesc);

            commandList->EndCommands();

            device->FlushCommandList(commandList);
            DestroyStagingBuffer(stagingAllocation);

        }

        void MemoryTransferManager::UploadImageData(void *data, Image* image, VkOffset3D offset, VkExtent3D extent,
            uint32_t layerOffset, uint32_t layerCount) {

            // Need graphics queue for mip generation
            auto commandList = device->GetCommandList(GraphicsQueue, true);
            VmaAllocator allocator = memoryManager->allocator;

            commandList->BeginCommands();

            auto formatSize = GetFormatSize(image->format);
            auto pixelCount = image->width * image->height * image->depth;
            auto stagingAllocation = CreateStagingBuffer(pixelCount * formatSize);

            void* destination;
            vmaMapMemory(allocator, stagingAllocation.allocation, &destination);
            std::memcpy(destination, data, pixelCount * formatSize);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            auto mipLevels = image->mipLevels;

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = mipLevels;
            range.baseArrayLayer = 0;
            range.layerCount = image->layers;

            VkImageMemoryBarrier imageBarrier = {};
            // Create first barrier to transition image
            {
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageBarrier.image = image->image;
                imageBarrier.subresourceRange = range;
                imageBarrier.srcAccessMask = 0;
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
            }

            // Copy buffer to image
            {
                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0;
                copyRegion.bufferImageHeight = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = layerOffset;
                copyRegion.imageSubresource.layerCount = layerCount;
                copyRegion.imageOffset = offset;
                copyRegion.imageExtent = extent;

                vkCmdCopyBufferToImage(commandList->commandBuffer, stagingAllocation.buffer, image->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
            }

            // Transition image again to make it shader readable
            {
                auto newLayout = mipLevels > 1 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL :
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                auto dstAccessMask = mipLevels > 1 ? VK_ACCESS_TRANSFER_READ_BIT :
                    VK_ACCESS_SHADER_READ_BIT;
                auto dstStageMask = mipLevels > 1 ? VK_PIPELINE_STAGE_TRANSFER_BIT :
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageBarrier.newLayout = newLayout;
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageBarrier.dstAccessMask = dstAccessMask;

                vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
                image->layout = newLayout;
                image->accessMask = dstAccessMask;
            }

            if (mipLevels > 1) GenerateMipMaps(image, commandList->commandBuffer);

            commandList->EndCommands();

            device->FlushCommandList(commandList);
            DestroyStagingBuffer(stagingAllocation);

        }

        void MemoryTransferManager::RetrieveImageData(void *data, Image *image, VkOffset3D offset,
            VkExtent3D extent, uint32_t layerOffset, uint32_t layerCount, bool block) {

            if (block) device->WaitForIdle();

            auto commandList = device->GetCommandList(GraphicsQueue, true);
            VmaAllocator allocator = memoryManager->allocator;

            commandList->BeginCommands();

            auto formatSize = GetFormatSize(image->format);
            auto pixelCount = image->width * image->height * image->depth;
            auto stagingAllocation = CreateStagingBuffer(pixelCount * formatSize);

            auto mipLevels = image->mipLevels;

            VkImageSubresourceRange range = {};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = mipLevels;
            range.baseArrayLayer = 0;
            range.layerCount = image->layers;

            VkImageMemoryBarrier imageBarrier = {};
            // Create first barrier to transition image
            {
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageBarrier.image = image->image;
                imageBarrier.subresourceRange = range;
                imageBarrier.srcAccessMask = 0;
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
            }

            // Copy image to buffer
            {
                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0;
                copyRegion.bufferImageHeight = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = layerOffset;
                copyRegion.imageSubresource.layerCount = layerCount;
                copyRegion.imageOffset = offset;
                copyRegion.imageExtent = extent;

                vkCmdCopyImageToBuffer(commandList->commandBuffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    stagingAllocation.buffer, 1, &copyRegion);
            }

            // Transition image again to make it shader readable
            {
                auto newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                auto dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                auto dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageBarrier.newLayout = newLayout;
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageBarrier.dstAccessMask = dstAccessMask;

                vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
                image->layout = newLayout;
                image->accessMask = dstAccessMask;
            }

            // Buffer barrier so we can map it afterwards
            {
                VkBufferMemoryBarrier bufferBarrier = Initializers::InitBufferMemoryBarrier(stagingAllocation.buffer,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

                vkCmdPipelineBarrier(commandList->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
            }

            void* src;
            vmaMapMemory(allocator, stagingAllocation.allocation, &src);
            std::memcpy(data, src, pixelCount * formatSize);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            commandList->EndCommands();

            device->FlushCommandList(commandList);
            DestroyStagingBuffer(stagingAllocation);

        }

        void MemoryTransferManager::GenerateMipMaps(Image *image, VkCommandBuffer cmd) {

            VkImageMemoryBarrier imageBarrier = {};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageBarrier.image = image->image;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange.baseArrayLayer = 0;
            imageBarrier.subresourceRange.layerCount = image->layers;
            imageBarrier.subresourceRange.levelCount = 1;

            auto mipWidth = int32_t(image->width);
            auto mipHeight = int32_t(image->height);
            auto mipDepth = int32_t(image->depth);

            for (uint32_t i = 1; i < image->mipLevels; i++) {
                imageBarrier.subresourceRange.baseMipLevel = i - 1;
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

                VkImageBlit blit = {};
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = image->layers;
                blit.dstOffsets[0] = { 0, 0, 0 };
                blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1,
                                       mipHeight > 1 ? mipHeight / 2 : 1,
                                       mipDepth > 1 ? mipDepth / 2 : 1 };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = image->layers;

                vkCmdBlitImage(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
                if (mipDepth > 1) mipDepth /= 2;
            }

            imageBarrier.subresourceRange.baseMipLevel = image->mipLevels - 1;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

            image->layout = imageBarrier.newLayout;
            image->accessMask = imageBarrier.dstAccessMask;

        }

        void MemoryTransferManager::ImmediateSubmit(QueueType queueType, std::function<void(CommandList*)> &&function) {

            auto commandList = device->GetCommandList(queueType, true);
            VmaAllocator allocator = memoryManager->allocator;

            commandList->BeginCommands();

            function(commandList);

            commandList->EndCommands();

            device->FlushCommandList(commandList);

        }

        MemoryTransferManager::StagingBufferAllocation MemoryTransferManager::CreateStagingBuffer(size_t size) {

            VkBufferCreateInfo stagingBufferInfo = {};
            stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.pNext = nullptr;

            stagingBufferInfo.size = size;
            stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            // We want to use memcpy on the buffer, therefore we need the sequential write flag
            VmaAllocationCreateInfo allocationCreateInfo = {};
            allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

            StagingBufferAllocation allocation;
            VK_CHECK(vmaCreateBuffer(memoryManager->allocator, &stagingBufferInfo, &allocationCreateInfo,
                &allocation.buffer, &allocation.allocation, nullptr));

            return allocation;

        }

        void MemoryTransferManager::DestroyStagingBuffer(StagingBufferAllocation &allocation) {

            vmaDestroyBuffer(memoryManager->allocator, allocation.buffer, allocation.allocation);

        }

    }

}