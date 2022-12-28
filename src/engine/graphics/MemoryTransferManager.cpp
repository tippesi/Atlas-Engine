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

        MemoryTransferManager::MemoryTransferManager(GraphicsDevice* device, MemoryManager *memManager,
            uint32_t transferQueueFamilyIndex, VkQueue transferQueue) : device(device), memoryManager(memManager),
            transferQueueFamilyIndex(transferQueueFamilyIndex), transferQueue(transferQueue) {

            VkCommandPoolCreateInfo poolCreateInfo = Initializers::InitCommandPoolCreateInfo(transferQueueFamilyIndex);
            VK_CHECK(vkCreateCommandPool(device->device, &poolCreateInfo, nullptr, &commandPool))

            VkCommandBufferAllocateInfo bufferAllocateInfo = Initializers::InitCommandBufferAllocateInfo(commandPool, 1);
            VK_CHECK(vkAllocateCommandBuffers(device->device, &bufferAllocateInfo, &commandBuffer))

            VkFenceCreateInfo fenceInfo = Initializers::InitFenceCreateInfo();
            VK_CHECK(vkCreateFence(device->device, &fenceInfo, nullptr, &fence))

        }

        MemoryTransferManager::~MemoryTransferManager() {

            vkDestroyFence(device->device, fence, nullptr);
            vkDestroyCommandPool(device->device, commandPool, nullptr);

        }

        void MemoryTransferManager::UploadBufferData(void *data, Buffer* destinationBuffer,
            VkBufferCopy bufferCopyDesc) {

            VmaAllocator allocator = memoryManager->allocator;

            auto stagingAllocation = CreateStagingBuffer(bufferCopyDesc.size);

            void* destination;
            vmaMapMemory(allocator, stagingAllocation.allocation, &destination);
            std::memcpy(destination, data, bufferCopyDesc.size);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            VkCommandBufferBeginInfo cmdBeginInfo =
                Initializers::InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

            vkCmdCopyBuffer(commandBuffer, stagingAllocation.buffer, destinationBuffer->buffer, 1, &bufferCopyDesc);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submit = Initializers::InitSubmitInfo(&commandBuffer);
            VK_CHECK(vkQueueSubmit(transferQueue, 1, &submit, fence));

            // We wait here until the operation is finished
            VK_CHECK(vkWaitForFences(device->device, 1, &fence, true, 9999999999))
            VK_CHECK(vkResetFences(device->device, 1, &fence))

            vkResetCommandPool(device->device, commandPool, 0);
            DestroyStagingBuffer(stagingAllocation);

        }

        void MemoryTransferManager::UploadBufferData(void *data, Buffer* destinationBuffer,
            VkBufferCopy bufferCopyDesc, VkCommandBuffer cmd) {

            VmaAllocator allocator = memoryManager->allocator;

            auto stagingAllocation = CreateStagingBuffer(bufferCopyDesc.size);

            void* destination;
            vmaMapMemory(allocator, stagingAllocation.allocation, &destination);
            std::memcpy(destination, data, bufferCopyDesc.size);
            vmaUnmapMemory(allocator, stagingAllocation.allocation);

            VkCommandBufferBeginInfo cmdBeginInfo =
                Initializers::InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

            vkCmdCopyBuffer(commandBuffer, stagingAllocation.buffer, destinationBuffer->buffer, 1, &bufferCopyDesc);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submit = Initializers::InitSubmitInfo(&commandBuffer);
            VK_CHECK(vkQueueSubmit(transferQueue, 1, &submit, fence));

            // We wait here until the operation is finished
            VK_CHECK(vkWaitForFences(device->device, 1, &fence, true, 9999999999))
            VK_CHECK(vkResetFences(device->device, 1, &fence))

            vkResetCommandPool(device->device, commandPool, 0);
            DestroyStagingBuffer(stagingAllocation);

            destinationBuffer->accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        }

        void MemoryTransferManager::UploadImageData(void *data, Image* image, VkOffset3D offset, VkExtent3D extent,
            uint32_t layerOffset, uint32_t layerCount) {

            VmaAllocator allocator = memoryManager->allocator;

            VkCommandBufferBeginInfo cmdBeginInfo =
                Initializers::InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

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
            range.baseArrayLayer = layerOffset;
            range.layerCount = layerCount;

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

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
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

                // We likely want to upload data to a layered image
                if (image->type != ImageType::Image3D && extent.depth > 1) {
                    copyRegion.imageExtent.depth = 1;
                }

                vkCmdCopyBufferToImage(commandBuffer, stagingAllocation.buffer, image->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
            }

            // Transition image again to make it shader readable
            {
                auto newLayout = mipLevels > 1 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL :
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                auto dstAccessMask = mipLevels > 1 ? VK_ACCESS_TRANSFER_READ_BIT :
                    VK_ACCESS_SHADER_READ_BIT;
                auto dstStageMask = mipLevels > 1 ? VK_PIPELINE_STAGE_TRANSFER_BIT :
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageBarrier.newLayout = newLayout;
                imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageBarrier.dstAccessMask = dstAccessMask;

                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
                image->layout = newLayout;
                image->accessMask = dstAccessMask;
            }

            if (mipLevels > 1) GenerateMipMaps(image, commandBuffer);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submit = Initializers::InitSubmitInfo(&commandBuffer);
            VK_CHECK(vkQueueSubmit(transferQueue, 1, &submit, fence));

            // We wait here until the operation is finished
            VK_CHECK(vkWaitForFences(device->device, 1, &fence, true, 9999999999))
            VK_CHECK(vkResetFences(device->device, 1, &fence))

            vkResetCommandPool(device->device, commandPool, 0);
            DestroyStagingBuffer(stagingAllocation);

            image->accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        }

        void MemoryTransferManager::UploadImageData(void *data, Image* image, VkOffset3D offset,
            VkExtent3D extent, VkCommandBuffer cmd) {



        }

        void MemoryTransferManager::RetrieveImageData(void *data, Image *image, VkOffset3D offset,
            VkExtent3D extent, bool block) {

            if (block) device->WaitForIdle();



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

        void MemoryTransferManager::ImmediateSubmit(std::function<void(VkCommandBuffer)> &&function) {

            VkCommandBufferBeginInfo cmdBeginInfo =
                Initializers::InitCommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            VK_CHECK(vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo));

            function(commandBuffer);

            VK_CHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submit = Initializers::InitSubmitInfo(&commandBuffer);
            VK_CHECK(vkQueueSubmit(transferQueue, 1, &submit, fence));

            // We wait here until the operation is finished
            vkWaitForFences(device->device, 1, &fence, true, 9999999999);
            vkResetFences(device->device, 1, &fence);

            vkResetCommandPool(device->device, commandPool, 0);

        }

        MemoryTransferManager::StagingBufferAllocation MemoryTransferManager::CreateStagingBuffer(size_t size) {

            VkBufferCreateInfo stagingBufferInfo = {};
            stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.pNext = nullptr;

            stagingBufferInfo.size = size;
            stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

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