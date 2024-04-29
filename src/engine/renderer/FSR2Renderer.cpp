#include "FSR2Renderer.h"

#include "graphics/Format.h"

#include "Clock.h"
#include "loader/ShaderLoader.h"

#include <string.h>

namespace Atlas::Renderer {

#define FSR2_MAX_QUEUED_FRAMES              ( 3)
#define FSR2_MAX_IMAGE_COPY_MIPS            (32)
#define FSR2_MAX_UNIFORM_BUFFERS            ( 4)
#define FSR2_MAX_BUFFERED_DESCRIPTORS       (FFX_FSR2_PASS_COUNT * FSR2_MAX_QUEUED_FRAMES)
#define FSR2_UBO_RING_BUFFER_SIZE           (FSR2_MAX_BUFFERED_DESCRIPTORS * FSR2_MAX_UNIFORM_BUFFERS)

	struct FSR2Resource {
		FfxResourceDescription desc = {};
		FfxResourceStates state;

		Ref<Graphics::Image> image = nullptr;
	};

	struct FSR2Pass {

		Ref<Graphics::Shader> shader = nullptr;
		Ref<Graphics::Pipeline> pipeline = nullptr;

	};

	class FSR2Context {
	public:
		FSR2Pass passes[FFX_FSR2_PASS_COUNT];

		std::vector<FSR2Resource> resources;
		std::vector<int32_t> freeResources;
		std::vector<int32_t> dynamicResources;

        std::vector<FfxGpuJobDescription> gpuJobs;

		std::vector<Buffer::UniformBuffer> uniformBuffers;
		size_t uniformBufferIndex = 0;

		Ref<Graphics::DescriptorPool> descriptorPool;

		Ref<Graphics::Sampler> pointSampler;
		Ref<Graphics::Sampler> linearSampler;

		std::vector<Graphics::ImageBarrier> imageBarriers;
		std::vector<Graphics::BufferBarrier> bufferBarriers;

		VkPipelineStageFlags srcStageMask = 0;
		VkPipelineStageFlags dstStageMask = 0;

		Graphics::GraphicsDevice* device = nullptr;

        FfxDeviceCapabilities deviceCapabilities;

		int32_t AddResource(const FSR2Resource& resource, bool isDynamic) {
			int32_t index = 0;
			if (freeResources.empty()) {
				index = uint32_t(resources.size());
				resources.push_back(resource);
			}
			else {
				index = freeResources.back();
				resources[index] = resource;
				freeResources.pop_back();
			}

			if (isDynamic)
				dynamicResources.push_back(index);

			AE_ASSERT(index < resources.size());

			return index;
		}

		void RemoveResource(int32_t index) {
			resources[index] = {};

			freeResources.push_back(index);
		}

		void ClearDynamicResources() {
			dynamicResources.clear();
		}

		Buffer::UniformBuffer& GetNextUniformBuffer() {
			auto index = uniformBufferIndex++;

			if (uniformBufferIndex >= FSR2_UBO_RING_BUFFER_SIZE)
				uniformBufferIndex = 0;

			return uniformBuffers[index];
		}

	};

	VkFormat GetFormatFromSurfaceFormat(FfxSurfaceFormat fmt) {
		switch (fmt) {
		case FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT:
			return VK_FORMAT_R16G16B16A16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16B16A16_UNORM:
			return VK_FORMAT_R16G16B16A16_UNORM;
		case FFX_SURFACE_FORMAT_R32G32_FLOAT:
			return VK_FORMAT_R32G32_SFLOAT;
		case FFX_SURFACE_FORMAT_R32_UINT:
			return VK_FORMAT_R32_UINT;
		case FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case FFX_SURFACE_FORMAT_R8G8B8A8_UNORM:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case FFX_SURFACE_FORMAT_R11G11B10_FLOAT:
			return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case FFX_SURFACE_FORMAT_R16G16_FLOAT:
			return VK_FORMAT_R16G16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16G16_UINT:
			return VK_FORMAT_R16G16_UINT;
		case FFX_SURFACE_FORMAT_R16_FLOAT:
			return VK_FORMAT_R16_SFLOAT;
		case FFX_SURFACE_FORMAT_R16_UINT:
			return VK_FORMAT_R16_UINT;
		case FFX_SURFACE_FORMAT_R16_UNORM:
			return VK_FORMAT_R16_UNORM;
		case FFX_SURFACE_FORMAT_R16_SNORM:
			return VK_FORMAT_R16_SNORM;
		case FFX_SURFACE_FORMAT_R8_UNORM:
			return VK_FORMAT_R8_UNORM;
		case FFX_SURFACE_FORMAT_R8G8_UNORM:
			return VK_FORMAT_R8G8_UNORM;
		case FFX_SURFACE_FORMAT_R32_FLOAT:
			return VK_FORMAT_R32_SFLOAT;
		case FFX_SURFACE_FORMAT_R8_UINT:
			return VK_FORMAT_R8_UINT;
		default:
			return VK_FORMAT_UNDEFINED;
		}
	}

	VkImageUsageFlags GetImageUsageFlagsFromResourceUsage(FfxResourceUsage flags) {
		VkImageUsageFlags ret = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (flags & FFX_RESOURCE_USAGE_RENDERTARGET) ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (flags & FFX_RESOURCE_USAGE_UAV) ret |= (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		return ret;
	}

	Graphics::ImageType GetImageTypeFromResourceType(FfxResourceType type) {
		switch (type) {
		case FFX_RESOURCE_TYPE_TEXTURE1D:
			return Graphics::ImageType::Image1D;
		case FFX_RESOURCE_TYPE_TEXTURE2D:
			return Graphics::ImageType::Image2D;
		case FFX_RESOURCE_TYPE_TEXTURE3D:
			return Graphics::ImageType::Image3D;
		default:
			return Graphics::ImageType::Image2D;
		}
	}

	VkImageLayout GetImageLayoutFromResourceState(FfxResourceStates state) {
		switch (state) {
		case FFX_RESOURCE_STATE_GENERIC_READ:
			return VK_IMAGE_LAYOUT_GENERAL;
		case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
			return VK_IMAGE_LAYOUT_GENERAL;
		case FFX_RESOURCE_STATE_COMPUTE_READ:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case FFX_RESOURCE_STATE_COPY_SRC:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case FFX_RESOURCE_STATE_COPY_DEST:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		default:
			return VK_IMAGE_LAYOUT_GENERAL;
		}
	}

	VkPipelineStageFlags GetPipelineStageFlagsFromResourceState(FfxResourceStates state) {
		switch (state) {
		case FFX_RESOURCE_STATE_GENERIC_READ:
		case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
		case FFX_RESOURCE_STATE_COMPUTE_READ:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case FFX_RESOURCE_STATE_COPY_SRC:
		case FFX_RESOURCE_STATE_COPY_DEST:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		default:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
	}

	VkAccessFlags GetAccessFlagsFromResourceState(FfxResourceStates state) {
		switch (state) {
		case FFX_RESOURCE_STATE_GENERIC_READ:
			return VK_ACCESS_SHADER_READ_BIT;
		case FFX_RESOURCE_STATE_UNORDERED_ACCESS:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case FFX_RESOURCE_STATE_COMPUTE_READ:
			return VK_ACCESS_SHADER_READ_BIT;
		case FFX_RESOURCE_STATE_COPY_SRC:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case FFX_RESOURCE_STATE_COPY_DEST:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		default:
			return VK_ACCESS_SHADER_READ_BIT;
		}
	}

	FfxSurfaceFormat GetSurfaceFormatVK(VkFormat format) {
		switch (format) {
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		case VK_FORMAT_R16G16B16A16_UNORM:
			return FFX_SURFACE_FORMAT_R16G16B16A16_UNORM;
		case VK_FORMAT_R32G32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32G32_FLOAT;
		case VK_FORMAT_R32_UINT:
			return FFX_SURFACE_FORMAT_R32_UINT;
		case VK_FORMAT_R8G8B8A8_UNORM:
			return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
			return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
		case VK_FORMAT_R16G16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16G16_FLOAT;
		case VK_FORMAT_R16G16_UINT:
			return FFX_SURFACE_FORMAT_R16G16_UINT;
		case VK_FORMAT_R16_SFLOAT:
			return FFX_SURFACE_FORMAT_R16_FLOAT;
		case VK_FORMAT_R16_UINT:
			return FFX_SURFACE_FORMAT_R16_UINT;
		case VK_FORMAT_R16_UNORM:
			return FFX_SURFACE_FORMAT_R16_UNORM;
		case VK_FORMAT_R16_SNORM:
			return FFX_SURFACE_FORMAT_R16_SNORM;
		case VK_FORMAT_R8_UNORM:
			return FFX_SURFACE_FORMAT_R8_UNORM;
		case VK_FORMAT_R32_SFLOAT:
			return FFX_SURFACE_FORMAT_R32_FLOAT;
		case VK_FORMAT_R8_UINT:
			return FFX_SURFACE_FORMAT_R8_UINT;
		default:
			return FFX_SURFACE_FORMAT_UNKNOWN;
		}
	}

	static FfxResource GetResource(Ref<Graphics::Image>& image, const wchar_t* name, FfxResourceStates state) {
		FfxResource resource = {};
		resource.resource = reinterpret_cast<void*>(&image);
		resource.state = state;
		resource.descriptorData = reinterpret_cast<uint64_t>(image->view);
		resource.description.flags = FFX_RESOURCE_FLAGS_NONE;
		resource.description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
		resource.description.width = image->width;
		resource.description.height = image->height;
		resource.description.depth = 1;
		resource.description.mipCount = 1;
		resource.description.format = GetSurfaceFormatVK(image->format);

		wcscpy(resource.name, name);

		switch (image->format) {
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			resource.isDepth = true;
			break;
		default:
			resource.isDepth = false;
			break;
		}

		return resource;
	}

    static FfxErrorCode RegisterResource(FfxFsr2Interface* backendInterface, const FfxResource* inFfxResource, FfxResourceInternal* outFfxResourceInternal) {
		AE_ASSERT(NULL != backendInterface);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		if (inFfxResource->resource == nullptr) {
			outFfxResourceInternal->internalIndex = -1;
			return FFX_OK;
		}

		AE_ASSERT(inFfxResource->description.type != FFX_RESOURCE_TYPE_BUFFER);

		FSR2Resource resource = {
			.desc = inFfxResource->description,
			.state = inFfxResource->state,
			.image = *reinterpret_cast<Ref<Graphics::Image>*>(inFfxResource->resource)
		};
		outFfxResourceInternal->internalIndex = fsr2Context->AddResource(resource, true);

		return FFX_OK;
	}

	// dispose dynamic resources: This should be called at the end of the frame
    static FfxErrorCode UnregisterResources(FfxFsr2Interface* backendInterface) {
		AE_ASSERT(NULL != backendInterface);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		for (auto index : fsr2Context->dynamicResources)
			fsr2Context->RemoveResource(index);

		fsr2Context->ClearDynamicResources();

		return FFX_OK;
	}

	static FfxErrorCode GetDeviceCapabilities(FfxFsr2Interface* backendInterface, FfxDeviceCapabilities* deviceCapabilities, FfxDevice device) {

        FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

        auto graphicsDevice = reinterpret_cast<Graphics::GraphicsDevice*>(device);
		const uint32_t defaultSubgroupSize = graphicsDevice->deviceProperties11.subgroupSize;

		// no shader model in vulkan so assume the minimum
		deviceCapabilities->minimumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
		deviceCapabilities->waveLaneCountMin = defaultSubgroupSize;
		deviceCapabilities->waveLaneCountMax = defaultSubgroupSize;
		deviceCapabilities->fp16Supported = graphicsDevice->support.shaderFloat16;
		deviceCapabilities->raytracingSupported = false;

		// check if extensions are enabled
        if (graphicsDevice->supportedExtensions.contains(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)) {
            auto& subgroupSizeControlProperties = graphicsDevice->subgroupSizeControlProperties;

            // NOTE: It's important to check requiredSubgroupSizeStages flags (and it's required by the spec).
            // As of August 2022, AMD's Vulkan drivers do not support subgroup size selection through Vulkan API
            // and this information is reported through requiredSubgroupSizeStages flags.
            if (subgroupSizeControlProperties.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) {
                deviceCapabilities->waveLaneCountMin = subgroupSizeControlProperties.minSubgroupSize;
                deviceCapabilities->waveLaneCountMax = subgroupSizeControlProperties.maxSubgroupSize;
            }
        }

        fsr2Context->deviceCapabilities = *deviceCapabilities;

		return FFX_OK;
	}

	static FfxErrorCode CreateBackendContext(FfxFsr2Interface* backendInterface, FfxDevice device) {
		AE_ASSERT(NULL != backendInterface);

		auto graphicsDevice = reinterpret_cast<Graphics::GraphicsDevice*>(device);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		fsr2Context->descriptorPool = graphicsDevice->CreateDescriptorPool({});

		Graphics::SamplerDesc samplerDesc = {
			.filter = VK_FILTER_NEAREST,
			.mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.maxLod = 1000,
		};
		fsr2Context->pointSampler = graphicsDevice->CreateSampler(samplerDesc);

		samplerDesc.filter = VK_FILTER_LINEAR;
		fsr2Context->linearSampler = graphicsDevice->CreateSampler(samplerDesc);

        fsr2Context->uniformBuffers.reserve(FSR2_UBO_RING_BUFFER_SIZE);

        for (uint32_t i = 0; i < FSR2_UBO_RING_BUFFER_SIZE; i++) {
            fsr2Context->uniformBuffers.emplace_back(256);
        }

		return FFX_OK;
	}

	static FfxErrorCode DestroyBackendContext(FfxFsr2Interface* backendInterface) {
		AE_ASSERT(NULL != backendInterface);

		// FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		return FFX_OK;
	}

	static FfxErrorCode CreateResource(FfxFsr2Interface* backendInterface, const FfxCreateResourceDescription* createResourceDescription,
		FfxResourceInternal* outResource) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != createResourceDescription);
		AE_ASSERT(NULL != outResource);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		FSR2Resource resource = {
			.desc = createResourceDescription->resourceDescription,
			.state = createResourceDescription->initalState
		};

		AE_ASSERT(createResourceDescription->resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER);

		auto mipLevels = createResourceDescription->resourceDescription.mipCount;

		Graphics::ImageDesc desc = {
			.usageFlags = GetImageUsageFlagsFromResourceUsage(createResourceDescription->usage),
			.type = GetImageTypeFromResourceType(createResourceDescription->resourceDescription.type),
			.width = createResourceDescription->resourceDescription.width,
			.height = createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE1D ? 1 : createResourceDescription->resourceDescription.height,
			.depth = createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE3D ? createResourceDescription->resourceDescription.depth : 1,
			.mipLevels = mipLevels,
			.format = GetFormatFromSurfaceFormat(createResourceDescription->resourceDescription.format),
			.mipMapping = mipLevels == 0 || mipLevels > 1,
			.data = createResourceDescription->initData,
		};

		auto expectedDataSize = Graphics::GetFormatSize(desc.format) * desc.depth * desc.width * desc.height;
		AE_ASSERT(desc.data == nullptr || createResourceDescription->initDataSize == expectedDataSize);

		auto image = fsr2Context->device->CreateImage(desc);
		AE_ASSERT(createResourceDescription->resourceDescription.mipCount == 0 || createResourceDescription->resourceDescription.mipCount == image->mipLevels);

		resource.image = image;
		resource.desc.mipCount = std::max(1u, image->mipLevels);

		outResource->internalIndex = fsr2Context->AddResource(resource, false);

		return FFX_OK;
	}

	static FfxResourceDescription GetResourceDescriptor(FfxFsr2Interface* backendInterface, FfxResourceInternal resource) {
		AE_ASSERT(NULL != backendInterface);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		FfxResourceDescription desc = {};

		if (resource.internalIndex != -1)
			desc = fsr2Context->resources[resource.internalIndex].desc;

		return desc;	
		
	}

	static FfxErrorCode ScheduleGpuJob(FfxFsr2Interface* backendInterface, const FfxGpuJobDescription* job) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != job);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

        fsr2Context->gpuJobs.push_back(*job);

		if (job->jobType == FFX_GPU_JOB_COMPUTE) {
			// needs to copy SRVs and UAVs in case they are on the stack only
			FfxComputeJobDescription* computeJob = &fsr2Context->gpuJobs.back().computeJobDescriptor;
			const uint32_t numConstBuffers = job->computeJobDescriptor.pipeline.constCount;
			for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < numConstBuffers; ++currentRootConstantIndex) {
				computeJob->cbs[currentRootConstantIndex].uint32Size = job->computeJobDescriptor.cbs[currentRootConstantIndex].uint32Size;
				std::memcpy(computeJob->cbs[currentRootConstantIndex].data, job->computeJobDescriptor.cbs[currentRootConstantIndex].data, computeJob->cbs[currentRootConstantIndex].uint32Size * sizeof(uint32_t));
			}
		}

		return FFX_OK;
	}

	static void AddBarrier(FSR2Context* fsr2Context, FfxResourceInternal* resource, FfxResourceStates newState) {
		AE_ASSERT(NULL != fsr2Context);
		AE_ASSERT(NULL != resource);

		auto& fsr2Resource = fsr2Context->resources[resource->internalIndex];

		AE_ASSERT(fsr2Resource.desc.type != FFX_RESOURCE_TYPE_BUFFER);

		auto image = fsr2Resource.image;
		FfxResourceStates& curState = fsr2Resource.state;

		auto newLayout = GetImageLayoutFromResourceState(newState);
		auto newAccessMask = GetAccessFlagsFromResourceState(newState);

		fsr2Context->imageBarriers.emplace_back(image, newLayout, newAccessMask);

		fsr2Context->srcStageMask |= GetPipelineStageFlagsFromResourceState(curState);
		fsr2Context->dstStageMask |= GetPipelineStageFlagsFromResourceState(newState);

		curState = newState;
	}

	static void FlushBarriers(FSR2Context* fsr2Context, Graphics::CommandList* commandList) {
		AE_ASSERT(NULL != fsr2Context);
		AE_ASSERT(NULL != commandList);

		if (!fsr2Context->imageBarriers.empty() || !fsr2Context->bufferBarriers.empty()) {
			commandList->PipelineBarrier(fsr2Context->imageBarriers, fsr2Context->bufferBarriers, fsr2Context->srcStageMask, fsr2Context->dstStageMask);

			fsr2Context->imageBarriers.clear();
			fsr2Context->bufferBarriers.clear();
			fsr2Context->srcStageMask = 0;
			fsr2Context->dstStageMask = 0;
		}
	}

	static FfxErrorCode ExecuteGpuJobCompute(FSR2Context* fsr2Context, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {

		FSR2Pass* pass = reinterpret_cast<FSR2Pass*>(job->computeJobDescriptor.pipeline.rootSignature);

		for (uint32_t uav = 0; uav < job->computeJobDescriptor.pipeline.uavCount; ++uav) {
			AddBarrier(fsr2Context, &job->computeJobDescriptor.uavs[uav], FFX_RESOURCE_STATE_UNORDERED_ACCESS);

			const auto& fsr2Resource = fsr2Context->resources[job->computeJobDescriptor.uavs[uav].internalIndex];
			const auto image = fsr2Resource.image;

			auto binding = job->computeJobDescriptor.pipeline.uavResourceBindings[uav].slotIndex;
			auto mipLevel = job->computeJobDescriptor.uavMip[uav];
        
            int32_t selectedMipLevel = std::min(int32_t(mipLevel), int32_t(image->mipMapViews.size()) - 1);
			commandList->BindImage(image, 1, binding, selectedMipLevel);
		}

		for (uint32_t srv = 0; srv < job->computeJobDescriptor.pipeline.srvCount; ++srv) {
			AddBarrier(fsr2Context, &job->computeJobDescriptor.srvs[srv], FFX_RESOURCE_STATE_COMPUTE_READ);

			const auto& fsr2Resource = fsr2Context->resources[job->computeJobDescriptor.srvs[srv].internalIndex];
			const auto image = fsr2Resource.image;

			auto binding = job->computeJobDescriptor.pipeline.srvResourceBindings[srv].slotIndex;
			commandList->BindImage(image, 1, binding);
		}

		for (uint32_t i = 0; i < job->computeJobDescriptor.pipeline.constCount; ++i) {
			auto& buffer = fsr2Context->GetNextUniformBuffer();
			buffer.SetData(job->computeJobDescriptor.cbs[i].data, 0, job->computeJobDescriptor.cbs[i].uint32Size * sizeof(uint32_t));

			auto binding = job->computeJobDescriptor.pipeline.cbResourceBindings[i].slotIndex;
			buffer.Bind(commandList, 1, binding);
		}

		commandList->BindSampler(fsr2Context->pointSampler, 0, 0);
		commandList->BindSampler(fsr2Context->linearSampler, 0, 1);

		// insert all the barriers 
		FlushBarriers(fsr2Context, commandList);

		commandList->BindPipeline(pass->pipeline);

		commandList->Dispatch( job->computeJobDescriptor.dimensions[0], job->computeJobDescriptor.dimensions[1], job->computeJobDescriptor.dimensions[2]);

		return FFX_OK;

	}

	static FfxErrorCode ExecuteGpuJobCopy(FSR2Context* fsr2Context, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {

		const auto& fsr2ResourceSrc = fsr2Context->resources[job->copyJobDescriptor.src.internalIndex];
		const auto& fsr2ResourceDst = fsr2Context->resources[job->copyJobDescriptor.dst.internalIndex];

		AddBarrier(fsr2Context, &job->copyJobDescriptor.src, FFX_RESOURCE_STATE_COPY_SRC);
		AddBarrier(fsr2Context, &job->copyJobDescriptor.dst, FFX_RESOURCE_STATE_COPY_DEST);
		FlushBarriers(fsr2Context, commandList);

        AE_ASSERT(fsr2ResourceSrc.desc.type != FFX_RESOURCE_TYPE_BUFFER && fsr2ResourceDst.desc.type != FFX_RESOURCE_TYPE_BUFFER);

        VkImageCopy             imageCopies[FSR2_MAX_IMAGE_COPY_MIPS];
        VkImage vkResourceSrc = fsr2ResourceSrc.image->image;
        VkImage vkResourceDst = fsr2ResourceDst.image->image;

        for (uint32_t mip = 0; mip < fsr2ResourceSrc.desc.mipCount; mip++) {
            VkImageSubresourceLayers subresourceLayers = {};

            subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceLayers.baseArrayLayer = 0;
            subresourceLayers.layerCount = 1;
            subresourceLayers.mipLevel = mip;

            VkOffset3D offset = {};

            offset.x = 0;
            offset.y = 0;
            offset.z = 0;

            VkExtent3D extent = {};

            extent.width = fsr2ResourceSrc.desc.width / (mip + 1);
            extent.height = fsr2ResourceSrc.desc.height / (mip + 1);
            extent.depth = fsr2ResourceSrc.desc.depth / (mip + 1);

            VkImageCopy& copyRegion = imageCopies[mip];

            copyRegion.srcSubresource = subresourceLayers;
            copyRegion.srcOffset = offset;
            copyRegion.dstSubresource = subresourceLayers;
            copyRegion.dstOffset = offset;
            copyRegion.extent = extent;
        }

        vkCmdCopyImage(commandList->commandBuffer, vkResourceSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkResourceDst,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fsr2ResourceSrc.desc.mipCount, imageCopies);

        return FFX_OK;
	}

	static FfxErrorCode ExecuteGpuJobClearFloat(FSR2Context* fsr2Context, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {
		uint32_t idx = job->clearJobDescriptor.target.internalIndex;

		const auto& fsr2Resource = fsr2Context->resources[idx];

		if (fsr2Resource.desc.type != FFX_RESOURCE_TYPE_BUFFER) {
			AddBarrier(fsr2Context, &job->clearJobDescriptor.target, FFX_RESOURCE_STATE_COPY_DEST);
			FlushBarriers(fsr2Context, commandList);

			VkClearColorValue clearColor = {};

			clearColor.float32[0] = job->clearJobDescriptor.color[0];
			clearColor.float32[1] = job->clearJobDescriptor.color[1];
			clearColor.float32[2] = job->clearJobDescriptor.color[2];
			clearColor.float32[3] = job->clearJobDescriptor.color[3];

			commandList->ClearImageColor(fsr2Resource.image, clearColor);
		}

		return FFX_OK;
	}

	static FfxErrorCode ExecuteGpuJobs(FfxFsr2Interface* backendInterface, FfxCommandList ffxCommandList) {
		AE_ASSERT(NULL != backendInterface);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		// execute all renderjobs
		for (auto& job : fsr2Context->gpuJobs) {
			Graphics::CommandList* commandList = reinterpret_cast<Graphics::CommandList*>(ffxCommandList);

			switch (job.jobType) {
			case FFX_GPU_JOB_CLEAR_FLOAT:
				ExecuteGpuJobClearFloat(fsr2Context, &job, commandList);
				break;
			case FFX_GPU_JOB_COPY:
				ExecuteGpuJobCopy(fsr2Context, &job, commandList);
				break;
			case FFX_GPU_JOB_COMPUTE:
				ExecuteGpuJobCompute(fsr2Context, &job, commandList);
				break;
			default:
				AE_ASSERT(false && "Unexpected job type");
			}
		}

		fsr2Context->gpuJobs.clear();

		return FFX_OK;
	}

	static FfxErrorCode DestroyResource(FfxFsr2Interface* backendInterface, FfxResourceInternal resource) {
		AE_ASSERT(backendInterface != nullptr);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);

		if (resource.internalIndex != -1) {
			fsr2Context->RemoveResource(resource.internalIndex);
		}

		return FFX_OK;
	}

	static FfxErrorCode CreatePipeline(FfxFsr2Interface* backendInterface, FfxFsr2Pass pass, const FfxPipelineDescription* pipelineDescription, FfxPipelineState* outPipeline) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != pipelineDescription);

		FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(backendInterface->scratchBuffer);
        auto graphicsDevice = fsr2Context->device;

		auto& fsr2Pass = fsr2Context->passes[pass];

		// query device capabilities 
		const FfxDeviceCapabilities& deviceCapabilities = fsr2Context->deviceCapabilities;
		const uint32_t defaultSubgroupSize = graphicsDevice->deviceProperties11.subgroupSize;

		// check if we can force wave64
		bool canForceWave64 = false;
		bool useLut = false;
		if (defaultSubgroupSize == 32 && deviceCapabilities.waveLaneCountMax == 64) {
			useLut = true;
			canForceWave64 = true;
		}
		else if (defaultSubgroupSize == 64) {
			useLut = true;
		}

		// check if we have 16bit floating point.
		bool supportedFP16 = graphicsDevice->support.shaderFloat16;

		if (pass == FFX_FSR2_PASS_ACCUMULATE || pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN) {
			// Workaround: Disable FP16 path for the accumulate pass on NVIDIA due to reduced occupancy and high VRAM throughput.
			if (graphicsDevice->deviceProperties.properties.vendorID == 0x10DE)
				supportedFP16 = false;
		}

		std::vector<std::string> macros = {
			"FFX_GPU",
			"FFX_GLSL 1",
			"FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS 1",
			"FFX_FSR2_OPTION_HDR_COLOR_INPUT 1"
		};

		if (useLut)
			macros.push_back("FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE 1");

		if (pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN)
			macros.push_back("FFX_FSR2_OPTION_APPLY_SHARPENING 1");

		// Some of the shaders don't support FP16 math
		if (supportedFP16 && pass != FFX_FSR2_PASS_RCAS && pass != FFX_FSR2_PASS_COMPUTE_LUMINANCE_PYRAMID)
			macros.push_back("FFX_HALF 1 ");

		auto variant = fsr2Pass.shader->GetVariant(macros);

		// All user space variables are in set 1, samplers are in set 0
		const auto& variantSet = variant->sets[1];
		const auto& variantLayout = variantSet.layout;

		// populate the pass.
		outPipeline->srvCount = variantLayout->size.sampledImageCount;
		outPipeline->uavCount = variantLayout->size.storageImageCount;
		outPipeline->constCount = variantLayout->size.dynamicUniformBufferCount + variantLayout->size.uniformBufferCount;

		uint32_t srvIndex = 0, uavIndex = 0, cbIndex = 0;
		for (uint32_t i = 0; i < BINDINGS_PER_DESCRIPTOR_SET; i++) {
			auto& binding = variantSet.bindings[i];
			if (!binding.valid)
				continue;

			auto& descBinding = binding.binding;

			std::wstring wideName = std::wstring(binding.name.begin(), binding.name.end());

			if (descBinding.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
				outPipeline->srvResourceBindings[srvIndex].slotIndex = descBinding.bindingIdx;
				wcscpy(outPipeline->srvResourceBindings[srvIndex].name, wideName.c_str());
				srvIndex++;
			}
			if (descBinding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
				outPipeline->uavResourceBindings[uavIndex].slotIndex = descBinding.bindingIdx;
				wcscpy(outPipeline->uavResourceBindings[uavIndex].name, wideName.c_str());
				uavIndex++;
			}
			if (descBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
				descBinding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				outPipeline->cbResourceBindings[cbIndex].slotIndex = descBinding.bindingIdx;
				wcscpy(outPipeline->cbResourceBindings[cbIndex].name, wideName.c_str());
				cbIndex++;
			}
		}

		auto& shaderStageCreateInfo = variant->stageCreateInfos.front();

		// set wave64 if possible
		VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeCreateInfo = {};
		if (canForceWave64) {
			subgroupSizeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO;
			subgroupSizeCreateInfo.requiredSubgroupSize = 64;

			shaderStageCreateInfo.pNext = &subgroupSizeCreateInfo;
		}

		Graphics::ComputePipelineDesc pipelineDesc = {
			.shader = variant
		};
		fsr2Pass.pipeline = fsr2Context->device->CreatePipeline(pipelineDesc);

		outPipeline->pipeline = reinterpret_cast<FfxPipeline>(fsr2Pass.pipeline->pipeline);
		outPipeline->rootSignature = reinterpret_cast<FfxRootSignature>(&fsr2Pass);

		return FFX_OK;
	}

	static FfxErrorCode DestroyPipeline(FfxFsr2Interface* backendInterface, FfxPipelineState* pipeline) {

		return FFX_OK;

	}

	void FSR2Renderer::Init(Graphics::GraphicsDevice* device) {

		this->device = device;

		GenerateShaders();

	}

	FSR2Renderer::~FSR2Renderer() {

		DestroyContext();

	}

	vec2 FSR2Renderer::GetJitter(const Ref<RenderTarget>& target, uint32_t index) const {

		vec2 jitter;

		const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(target->GetScaledWidth(), target->GetWidth());
		ffxFsr2GetJitterOffset(&jitter.x, &jitter.y, index, jitterPhaseCount);

		return vec2(2.0f * jitter.x / (float)target->GetScaledWidth(), 2.0f * jitter.y / (float)target->GetScaledHeight());

	}

	void FSR2Renderer::Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) {

		if (!initParams.callbacks.scratchBuffer ||
            initParams.displaySize.width != target->GetWidth() ||
            initParams.displaySize.height != target->GetHeight() ||
            initParams.maxRenderSize.width != target->GetScaledWidth() ||
            initParams.maxRenderSize.height != target->GetScaledHeight()) {
			DestroyContext();
			CreateContext(target, device);
		}

		Graphics::Profiler::BeginQuery("FSR2");

		auto& postProcessing = scene->postProcessing;
		auto& sharpen = postProcessing.sharpen;
		auto& camera = scene->GetMainCamera();

		auto targetData = target->GetData(FULL_RES);

		auto colorImage = target->lightingTexture.image;
		auto velocityImage = target->GetVelocity()->image;
		auto depthImage = targetData->depthTexture->image;
		auto outputImage = target->hdrTexture.image;

		auto& taa = scene->postProcessing.taa;

		std::vector<Graphics::BufferBarrier> bufferBarriers;
		std::vector<Graphics::ImageBarrier> imageBarriers;
		imageBarriers.push_back({ outputImage,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT });
		commandList->PipelineBarrier(imageBarriers, bufferBarriers);

		FfxFsr2DispatchDescription dispatchParameters = {};
		dispatchParameters.commandList = commandList;
		dispatchParameters.color = GetResource(colorImage, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
		dispatchParameters.depth = GetResource(depthImage, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
		dispatchParameters.motionVectors = GetResource(velocityImage, L"FSR2_InputMotionVectors", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.exposure = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_InputExposure", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.reactive = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyInputReactiveMap", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.transparencyAndComposition = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyTransparencyAndCompositionMap", FFX_RESOURCE_STATE_COMPUTE_READ);

		dispatchParameters.output = GetResource(outputImage, L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		dispatchParameters.jitterOffset.x = camera.GetJitter().x * float(target->GetScaledWidth()) * 0.5f;
		dispatchParameters.jitterOffset.y = camera.GetJitter().y * float(target->GetScaledHeight()) * 0.5f;
		dispatchParameters.motionVectorScale.x = float(target->GetScaledWidth());
		dispatchParameters.motionVectorScale.y = float(target->GetScaledHeight());
		dispatchParameters.reset = !target->HasHistory();
		dispatchParameters.enableSharpening = sharpen.enable;
		dispatchParameters.sharpness = sharpen.factor;
		dispatchParameters.frameTimeDelta = Clock::GetDelta() * 1000.0f;
		dispatchParameters.preExposure = 1.0f;
		dispatchParameters.renderSize.width = uint32_t(target->GetScaledWidth());
		dispatchParameters.renderSize.height = uint32_t(target->GetScaledHeight());
		dispatchParameters.cameraFar = camera.farPlane;
		dispatchParameters.cameraNear = camera.nearPlane;
		dispatchParameters.cameraFovAngleVertical = glm::radians(camera.fieldOfView);

		FfxErrorCode errorCode = ffxFsr2ContextDispatch(&context, &dispatchParameters);
		AE_ASSERT(errorCode == FFX_OK);

		commandList->ImageMemoryBarrier(outputImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		Graphics::Profiler::EndQuery();

	}

	void FSR2Renderer::CreateContext(const Ref<RenderTarget>& target, Graphics::GraphicsDevice* device) {

		FSR2Context* fsr2Context = new FSR2Context();
		fsr2Context->device = device;

		initParams.flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE;
        initParams.flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;

		for (int32_t i = 0; i < FFX_FSR2_PASS_COUNT; i++) {
			auto& pass = fsr2Context->passes[i];

			pass.shader = shaders[i];
		}

        initParams.device = device;
        initParams.displaySize.width = uint32_t(target->GetWidth());
        initParams.displaySize.height = uint32_t(target->GetHeight());
        initParams.maxRenderSize.width = uint32_t(target->GetScaledWidth());
        initParams.maxRenderSize.height = uint32_t(target->GetScaledHeight());

        initParams.callbacks.fpCreatePipeline = CreatePipeline;
        initParams.callbacks.fpDestroyPipeline = DestroyPipeline;
        initParams.callbacks.fpGetDeviceCapabilities = GetDeviceCapabilities;
        initParams.callbacks.fpCreateBackendContext = CreateBackendContext;
        initParams.callbacks.fpDestroyBackendContext = DestroyBackendContext;
        initParams.callbacks.fpCreateResource = CreateResource;
        initParams.callbacks.fpRegisterResource = RegisterResource;
        initParams.callbacks.fpUnregisterResources = UnregisterResources;
        initParams.callbacks.fpGetResourceDescription = GetResourceDescriptor;
        initParams.callbacks.fpDestroyResource = DestroyResource;
        initParams.callbacks.fpScheduleGpuJob = ScheduleGpuJob;
        initParams.callbacks.fpExecuteGpuJobs = ExecuteGpuJobs;
        initParams.callbacks.scratchBuffer = fsr2Context;
        initParams.callbacks.scratchBufferSize = sizeof(FSR2Context);

		ffxFsr2ContextCreate(&context, &initParams);

	}

	void FSR2Renderer::DestroyContext() {

		device->WaitForIdle();

		if (initParams.callbacks.scratchBuffer != nullptr) {
			ffxFsr2ContextDestroy(&context);
			FSR2Context* fsr2Context = reinterpret_cast<FSR2Context*>(initParams.callbacks.scratchBuffer);
			delete fsr2Context;

            initParams.callbacks.scratchBuffer = nullptr;
		}

	}

	void FSR2Renderer::GenerateShaders() {

		shaders.resize(FFX_FSR2_PASS_COUNT);

		auto loadShader = [&](const std::string& filename, FfxFsr2Pass pass) {
			auto stage = Loader::ShaderLoader::LoadFile(filename, VK_SHADER_STAGE_COMPUTE_BIT);
			Graphics::ShaderDesc desc{ .stages = { stage } };
			shaders[pass] = device->CreateShader(desc);
			};

		loadShader("fsr2/ffx_fsr2_depth_clip_pass.glsl", FFX_FSR2_PASS_DEPTH_CLIP);
		loadShader("fsr2/ffx_fsr2_reconstruct_previous_depth_pass.glsl", FFX_FSR2_PASS_RECONSTRUCT_PREVIOUS_DEPTH);
		loadShader("fsr2/ffx_fsr2_lock_pass.glsl", FFX_FSR2_PASS_LOCK);
		loadShader("fsr2/ffx_fsr2_accumulate_pass.glsl", FFX_FSR2_PASS_ACCUMULATE);
		loadShader("fsr2/ffx_fsr2_accumulate_pass.glsl", FFX_FSR2_PASS_ACCUMULATE_SHARPEN);
		loadShader("fsr2/ffx_fsr2_rcas_pass.glsl", FFX_FSR2_PASS_RCAS);
		loadShader("fsr2/ffx_fsr2_compute_luminance_pyramid_pass.glsl", FFX_FSR2_PASS_COMPUTE_LUMINANCE_PYRAMID);
		loadShader("fsr2/ffx_fsr2_autogen_reactive_pass.glsl", FFX_FSR2_PASS_GENERATE_REACTIVE);
		loadShader("fsr2/ffx_fsr2_tcr_autogen_pass.glsl", FFX_FSR2_PASS_TCR_AUTOGENERATE);

	}

}
