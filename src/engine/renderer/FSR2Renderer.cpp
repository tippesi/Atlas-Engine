#include "FSR2Renderer.h"

#include "graphics/Format.h"

#include "Clock.h"
#include "loader/ShaderLoader.h"

#include <volk.h>

#include <fsr2/ffx-fsr2-api/ffx_fsr2_interface.h>
#include "fsr2/ffx-fsr2-api/ffx_fsr2.h"
#include <fsr2/ffx-fsr2-api/ffx_fsr2_private.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <codecvt>
#include <locale>

namespace Atlas::Renderer {

#define FSR2_MAX_QUEUED_FRAMES              ( 3)
#define FSR2_MAX_RESOURCE_COUNT             (64)
#define FSR2_MAX_STAGING_RESOURCE_COUNT     ( 8)
#define FSR2_MAX_BARRIERS                   (16)
#define FSR2_MAX_GPU_JOBS                   (32)
#define FSR2_MAX_IMAGE_COPY_MIPS            (32)
#define FSR2_MAX_SAMPLERS                   ( 2)
#define FSR2_MAX_UNIFORM_BUFFERS            ( 4)
#define FSR2_MAX_IMAGE_VIEWS                (32)
#define FSR2_MAX_BUFFERED_DESCRIPTORS       (FFX_FSR2_PASS_COUNT * FSR2_MAX_QUEUED_FRAMES)
#define FSR2_UBO_RING_BUFFER_SIZE           (FSR2_MAX_BUFFERED_DESCRIPTORS * FSR2_MAX_UNIFORM_BUFFERS)
#define FSR2_UBO_MEMORY_BLOCK_SIZE          (FSR2_UBO_RING_BUFFER_SIZE * 256)

	struct FSR2Resource {
		FfxResourceDescription desc = {};
		FfxResourceStates state;

		Ref<Graphics::Image> image = nullptr;

		bool isDynamic = false;
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

			resources[index].isDynamic = isDynamic;

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

	typedef struct BackendContext_VK {

		typedef struct UniformBuffer {
			VkBuffer bufferResource;
			uint8_t* pData;
		} UniformBuffer;

		typedef struct PipelineLayout {
			VkDescriptorSetLayout descriptorSetLayout;
			VkDescriptorSet       descriptorSets[FSR2_MAX_QUEUED_FRAMES];
			uint32_t              descriptorSetIndex;
			VkPipelineLayout      pipelineLayout;
			uint32_t 			  passIndex;
		} PipelineLayout;

		VkPhysicalDevice        physicalDevice = nullptr;
		VkDevice                device = nullptr;

		uint32_t                gpuJobCount = 0;
		FfxGpuJobDescription    gpuJobs[FSR2_MAX_GPU_JOBS] = {};

		VkDescriptorSetLayout   samplerDescriptorSetLayout = nullptr;
		VkDescriptorSet         samplerDescriptorSet = nullptr;
		uint32_t                allocatedPipelineLayoutCount = 0;
		PipelineLayout          pipelineLayouts[FFX_FSR2_PASS_COUNT] = {};

		uint32_t                numDeviceExtensions = 0;
		VkExtensionProperties* extensionProperties = nullptr;

		FSR2Context* context;

	} BackendContext_VK;

	// prototypes for functions in the interface
	FfxErrorCode GetDeviceCapabilitiesVK(FfxFsr2Interface* backendInterface, FfxDeviceCapabilities* deviceCapabilities, FfxDevice device);
	FfxErrorCode CreateBackendContextVK(FfxFsr2Interface* backendInterface, FfxDevice device);
	FfxErrorCode DestroyBackendContextVK(FfxFsr2Interface* backendInterface);
	FfxErrorCode CreateResourceVK(FfxFsr2Interface* backendInterface, const FfxCreateResourceDescription* desc, FfxResourceInternal* outResource);
	FfxErrorCode RegisterResourceVK(FfxFsr2Interface* backendInterface, const FfxResource* inResource, FfxResourceInternal* outResourceInternal);
	FfxErrorCode UnregisterResourcesVK(FfxFsr2Interface* backendInterface);
	FfxResourceDescription GetResourceDescriptorVK(FfxFsr2Interface* backendInterface, FfxResourceInternal resource);
	FfxErrorCode DestroyResourceVK(FfxFsr2Interface* backendInterface, FfxResourceInternal resource);
	FfxErrorCode ScheduleGpuJobVK(FfxFsr2Interface* backendInterface, const FfxGpuJobDescription* job);
	FfxErrorCode ExecuteGpuJobsVK(FfxFsr2Interface* backendInterface, FfxCommandList commandList);

	FFX_API size_t Fsr2GetScratchMemorySizeVK(VkPhysicalDevice physicalDevice) {
		uint32_t numExtensions = 0;

		if (physicalDevice)
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExtensions, nullptr);

		return FFX_ALIGN_UP(sizeof(BackendContext_VK) + sizeof(VkExtensionProperties) * numExtensions, sizeof(uint64_t));
	}

	FfxErrorCode Fsr2GetInterfaceVK(
		FfxFsr2Interface* outInterface,
		void* scratchBuffer,
		size_t scratchBufferSize,
		FSR2Context* userData,
		Graphics::GraphicsDevice* device,
		PFN_vkGetDeviceProcAddr getDeviceProcAddr) {
		outInterface->fpGetDeviceCapabilities = GetDeviceCapabilitiesVK;
		outInterface->fpCreateBackendContext = CreateBackendContextVK;
		outInterface->fpDestroyBackendContext = DestroyBackendContextVK;
		outInterface->fpCreateResource = CreateResourceVK;
		outInterface->fpRegisterResource = RegisterResourceVK;
		outInterface->fpUnregisterResources = UnregisterResourcesVK;
		outInterface->fpGetResourceDescription = GetResourceDescriptorVK;
		outInterface->fpDestroyResource = DestroyResourceVK;
		outInterface->fpScheduleGpuJob = ScheduleGpuJobVK;
		outInterface->fpExecuteGpuJobs = ExecuteGpuJobsVK;
		outInterface->scratchBuffer = scratchBuffer;
		outInterface->scratchBufferSize = scratchBufferSize;

		BackendContext_VK* context = (BackendContext_VK*)scratchBuffer;

		context->physicalDevice = device->physicalDevice;
		context->context = userData;

		return FFX_OK;
	}

	VkFormat getVKFormatFromSurfaceFormat(FfxSurfaceFormat fmt) {
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

	VkImageUsageFlags getVKImageUsageFlagsFromResourceUsage(FfxResourceUsage flags) {
		VkImageUsageFlags ret = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (flags & FFX_RESOURCE_USAGE_RENDERTARGET) ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (flags & FFX_RESOURCE_USAGE_UAV) ret |= (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		return ret;
	}

	VkBufferUsageFlags getVKBufferUsageFlagsFromResourceUsage(FfxResourceUsage flags) {
		if (flags & FFX_RESOURCE_USAGE_UAV)
			return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		else
			return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	VkImageType getVKImageTypeFromResourceType(FfxResourceType type) {
		switch (type) {
		case(FFX_RESOURCE_TYPE_TEXTURE1D):
			return VK_IMAGE_TYPE_1D;
		case(FFX_RESOURCE_TYPE_TEXTURE2D):
			return VK_IMAGE_TYPE_2D;
		case(FFX_RESOURCE_TYPE_TEXTURE3D):
			return VK_IMAGE_TYPE_3D;
		default:
			return VK_IMAGE_TYPE_MAX_ENUM;
		}
	}

	Graphics::ImageType GetImageTypeFromResourceType(FfxResourceType type) {
		switch (type) {
		case(FFX_RESOURCE_TYPE_TEXTURE1D):
			return Graphics::ImageType::Image1D;
		case(FFX_RESOURCE_TYPE_TEXTURE2D):
			return Graphics::ImageType::Image2D;
		case(FFX_RESOURCE_TYPE_TEXTURE3D):
			return Graphics::ImageType::Image3D;
		default:
			return Graphics::ImageType::Image2D;
		}
	}

	VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state) {
		switch (state) {

		case(FFX_RESOURCE_STATE_GENERIC_READ):
			return VK_IMAGE_LAYOUT_GENERAL;
		case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
			return VK_IMAGE_LAYOUT_GENERAL;
		case(FFX_RESOURCE_STATE_COMPUTE_READ):
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case FFX_RESOURCE_STATE_COPY_SRC:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case FFX_RESOURCE_STATE_COPY_DEST:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		default:
			return VK_IMAGE_LAYOUT_GENERAL;
		}
	}

	VkPipelineStageFlags getVKPipelineStageFlagsFromResourceState(FfxResourceStates state) {
		switch (state) {

		case(FFX_RESOURCE_STATE_GENERIC_READ):
		case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
		case(FFX_RESOURCE_STATE_COMPUTE_READ):
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		case FFX_RESOURCE_STATE_COPY_SRC:
		case FFX_RESOURCE_STATE_COPY_DEST:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		default:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
	}

	VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state) {
		switch (state) {

		case(FFX_RESOURCE_STATE_GENERIC_READ):
			return VK_ACCESS_SHADER_READ_BIT;
		case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case(FFX_RESOURCE_STATE_COMPUTE_READ):
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

	static uint32_t getDefaultSubgroupSize(const BackendContext_VK* backendContext) {
		VkPhysicalDeviceVulkan11Properties vulkan11Properties = {};
		vulkan11Properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;

		VkPhysicalDeviceProperties2 deviceProperties2 = {};
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &vulkan11Properties;
		vkGetPhysicalDeviceProperties2(backendContext->physicalDevice, &deviceProperties2);
		AE_ASSERT(vulkan11Properties.subgroupSize == 32 || vulkan11Properties.subgroupSize == 64); // current desktop market

		return vulkan11Properties.subgroupSize;
	}

	// Create a FfxFsr2Device from a VkDevice
	FfxDevice GetDeviceVK(VkDevice vkDevice) {
		AE_ASSERT(NULL != vkDevice);
		return reinterpret_cast<FfxDevice>(vkDevice);
	}

	FfxResource GetResource(FfxFsr2Context* context, Ref<Graphics::Image>& image, const wchar_t* name, FfxResourceStates state) {
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
		{
			resource.isDepth = true;
			break;
		}
		default:
		{
			resource.isDepth = false;
			break;
		}
		}

		return resource;
	}

	FfxErrorCode RegisterResourceVK(FfxFsr2Interface* backendInterface, const FfxResource* inFfxResource, FfxResourceInternal* outFfxResourceInternal) {
		AE_ASSERT(NULL != backendInterface);

		BackendContext_VK* backendContext = (BackendContext_VK*)(backendInterface->scratchBuffer);
		FSR2Context* fsr2Context = backendContext->context;

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
	FfxErrorCode UnregisterResourcesVK(FfxFsr2Interface* backendInterface) {
		AE_ASSERT(NULL != backendInterface);

		BackendContext_VK* backendContext = (BackendContext_VK*)(backendInterface->scratchBuffer);
		FSR2Context* fsr2Context = backendContext->context;

		for (auto index : fsr2Context->dynamicResources)
			fsr2Context->RemoveResource(index);

		fsr2Context->ClearDynamicResources();

		return FFX_OK;
	}

	FfxErrorCode GetDeviceCapabilitiesVK(FfxFsr2Interface* backendInterface, FfxDeviceCapabilities* deviceCapabilities, FfxDevice device) {
		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

		const uint32_t defaultSubgroupSize = getDefaultSubgroupSize(backendContext);

		// no shader model in vulkan so assume the minimum
		deviceCapabilities->minimumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
		deviceCapabilities->waveLaneCountMin = defaultSubgroupSize;
		deviceCapabilities->waveLaneCountMax = defaultSubgroupSize;
		deviceCapabilities->fp16Supported = false;
		deviceCapabilities->raytracingSupported = false;

		// check if extensions are enabled

		for (uint32_t i = 0; i < backendContext->numDeviceExtensions; i++) {
			if (strcmp(backendContext->extensionProperties[i].extensionName, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) == 0) {
				// check if we the max subgroup size allows us to use wave64
				VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties = {};
				subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;

				VkPhysicalDeviceProperties2 deviceProperties2 = {};
				deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				deviceProperties2.pNext = &subgroupSizeControlProperties;
				vkGetPhysicalDeviceProperties2(backendContext->physicalDevice, &deviceProperties2);

				// NOTE: It's important to check requiredSubgroupSizeStages flags (and it's required by the spec).
				// As of August 2022, AMD's Vulkan drivers do not support subgroup size selection through Vulkan API
				// and this information is reported through requiredSubgroupSizeStages flags.
				if (subgroupSizeControlProperties.requiredSubgroupSizeStages & VK_SHADER_STAGE_COMPUTE_BIT) {
					deviceCapabilities->waveLaneCountMin = subgroupSizeControlProperties.minSubgroupSize;
					deviceCapabilities->waveLaneCountMax = subgroupSizeControlProperties.maxSubgroupSize;
				}
			}
			if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) == 0) {
				// check for fp16 support
				VkPhysicalDeviceShaderFloat16Int8Features shaderFloat18Int8Features = {};
				shaderFloat18Int8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;

				VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
				physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				physicalDeviceFeatures2.pNext = &shaderFloat18Int8Features;

				vkGetPhysicalDeviceFeatures2(backendContext->physicalDevice, &physicalDeviceFeatures2);

				deviceCapabilities->fp16Supported = (bool)shaderFloat18Int8Features.shaderFloat16;
			}
			if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
				// check for ray tracing support 
				VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
				accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

				VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
				physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				physicalDeviceFeatures2.pNext = &accelerationStructureFeatures;

				vkGetPhysicalDeviceFeatures2(backendContext->physicalDevice, &physicalDeviceFeatures2);

				deviceCapabilities->raytracingSupported = (bool)accelerationStructureFeatures.accelerationStructure;
			}
		}

		return FFX_OK;
	}

	FfxErrorCode CreateBackendContextVK(FfxFsr2Interface* backendInterface, FfxDevice device) {
		AE_ASSERT(NULL != backendInterface);

		auto graphicsDevice = reinterpret_cast<Graphics::GraphicsDevice*>(device);
		VkDevice vkDevice = graphicsDevice->device;

		// set up some internal resources we need (space for resource views and constant buffers)
		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;
		backendContext->extensionProperties = (VkExtensionProperties*)(backendContext + 1);

		// make sure the extra parameters were already passed in
		AE_ASSERT(backendContext->physicalDevice != NULL);

		// if vkGetDeviceProcAddr is NULL, use the one from the vulkan header
		if (vkGetDeviceProcAddr == NULL)
			vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		if (vkDevice != NULL) {
			backendContext->device = vkDevice;
		}

		// enumerate all the device extensions 
		backendContext->numDeviceExtensions = 0;
		vkEnumerateDeviceExtensionProperties(backendContext->physicalDevice, nullptr, &backendContext->numDeviceExtensions, nullptr);
		vkEnumerateDeviceExtensionProperties(backendContext->physicalDevice, nullptr, &backendContext->numDeviceExtensions, backendContext->extensionProperties);

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

		{
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};

			VkDescriptorSetLayoutBinding bindings[] = {
				{ 0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &fsr2Context->pointSampler->sampler },
				{ 1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &fsr2Context->linearSampler->sampler },
			};

			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.bindingCount = 2;
			descriptorSetLayoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(backendContext->device, &descriptorSetLayoutCreateInfo, NULL, &backendContext->samplerDescriptorSetLayout) != VK_SUCCESS) {
				return FFX_ERROR_BACKEND_API_ERROR;
			}
		}

		{
			VkDescriptorSetAllocateInfo allocateInfo = {};

			allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocateInfo.descriptorPool = fsr2Context->descriptorPool->GetNativePool();
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &backendContext->samplerDescriptorSetLayout;

			vkAllocateDescriptorSets(backendContext->device, &allocateInfo, &backendContext->samplerDescriptorSet);
		}

		// allocate ring buffer of uniform buffers
		{
			fsr2Context->uniformBuffers.reserve(FSR2_UBO_RING_BUFFER_SIZE);

			for (uint32_t i = 0; i < FSR2_UBO_RING_BUFFER_SIZE; i++) {
				fsr2Context->uniformBuffers.emplace_back(256);
			}
		}

		backendContext->gpuJobCount = 0;
		backendContext->allocatedPipelineLayoutCount = 0;

		return FFX_OK;
	}

	FfxErrorCode DestroyBackendContextVK(FfxFsr2Interface* backendInterface) {
		AE_ASSERT(NULL != backendInterface);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

		vkDestroyDescriptorSetLayout(backendContext->device, backendContext->samplerDescriptorSetLayout, nullptr);
		backendContext->samplerDescriptorSet = nullptr;
		backendContext->samplerDescriptorSetLayout = nullptr;

		if (backendContext->device != nullptr) {

			backendContext->device = nullptr;
		}

		return FFX_OK;
	}

	// create a internal resource that will stay alive until effect gets shut down
	FfxErrorCode CreateResourceVK(
		FfxFsr2Interface* backendInterface,
		const FfxCreateResourceDescription* createResourceDescription,
		FfxResourceInternal* outResource) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != createResourceDescription);
		AE_ASSERT(NULL != outResource);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;

		FSR2Resource resource = {
			.desc = createResourceDescription->resourceDescription,
			.state = createResourceDescription->initalState
		};

		AE_ASSERT(createResourceDescription->resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER);

		auto mipLevels = createResourceDescription->resourceDescription.mipCount;

		Graphics::ImageDesc desc = {
			.usageFlags = getVKImageUsageFlagsFromResourceUsage(createResourceDescription->usage),
			.type = GetImageTypeFromResourceType(createResourceDescription->resourceDescription.type),
			.width = createResourceDescription->resourceDescription.width,
			.height = createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE1D ? 1 : createResourceDescription->resourceDescription.height,
			.depth = createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE3D ? createResourceDescription->resourceDescription.depth : 1,
			.mipLevels = mipLevels,
			.format = getVKFormatFromSurfaceFormat(createResourceDescription->resourceDescription.format),
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

	FfxResourceDescription GetResourceDescriptorVK(FfxFsr2Interface* backendInterface, FfxResourceInternal resource) {
		AE_ASSERT(NULL != backendInterface);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;

		if (resource.internalIndex != -1) {
			FfxResourceDescription desc = fsr2Context->resources[resource.internalIndex].desc;
			return desc;
		}
		else {
			FfxResourceDescription desc = {};
			return desc;
		}
	}

	FfxErrorCode ScheduleGpuJobVK(FfxFsr2Interface* backendInterface, const FfxGpuJobDescription* job) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != job);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

		AE_ASSERT(backendContext->gpuJobCount < FSR2_MAX_GPU_JOBS);

		backendContext->gpuJobs[backendContext->gpuJobCount] = *job;

		if (job->jobType == FFX_GPU_JOB_COMPUTE) {

			// needs to copy SRVs and UAVs in case they are on the stack only
			FfxComputeJobDescription* computeJob = &backendContext->gpuJobs[backendContext->gpuJobCount].computeJobDescriptor;
			const uint32_t numConstBuffers = job->computeJobDescriptor.pipeline.constCount;
			for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < numConstBuffers; ++currentRootConstantIndex) {
				computeJob->cbs[currentRootConstantIndex].uint32Size = job->computeJobDescriptor.cbs[currentRootConstantIndex].uint32Size;
				memcpy(computeJob->cbs[currentRootConstantIndex].data, job->computeJobDescriptor.cbs[currentRootConstantIndex].data, computeJob->cbs[currentRootConstantIndex].uint32Size * sizeof(uint32_t));
			}
		}

		backendContext->gpuJobCount++;

		return FFX_OK;
	}

	void addBarrier(BackendContext_VK* backendContext, FfxResourceInternal* resource, FfxResourceStates newState) {
		AE_ASSERT(NULL != backendContext);
		AE_ASSERT(NULL != resource);

		FSR2Context* fsr2Context = backendContext->context;
		auto& fsr2Resource = fsr2Context->resources[resource->internalIndex];

		AE_ASSERT(fsr2Resource.desc.type != FFX_RESOURCE_TYPE_BUFFER);

		auto image = fsr2Resource.image;
		FfxResourceStates& curState = fsr2Resource.state;

		auto newLayout = getVKImageLayoutFromResourceState(newState);
		auto newAccessMask = getVKAccessFlagsFromResourceState(newState);

		fsr2Context->imageBarriers.emplace_back(image, newLayout, newAccessMask);

		fsr2Context->srcStageMask |= getVKPipelineStageFlagsFromResourceState(curState);
		fsr2Context->dstStageMask |= getVKPipelineStageFlagsFromResourceState(newState);

		curState = newState;
	}

	void flushBarriers(BackendContext_VK* backendContext, Graphics::CommandList* commandList) {
		AE_ASSERT(NULL != backendContext);
		AE_ASSERT(NULL != commandList);

		FSR2Context* fsr2Context = backendContext->context;

		if (!fsr2Context->imageBarriers.empty() || !fsr2Context->bufferBarriers.empty()) {
			commandList->PipelineBarrier(fsr2Context->imageBarriers, fsr2Context->bufferBarriers, fsr2Context->srcStageMask, fsr2Context->dstStageMask);

			fsr2Context->imageBarriers.clear();
			fsr2Context->bufferBarriers.clear();
			fsr2Context->srcStageMask = 0;
			fsr2Context->dstStageMask = 0;
		}
	}

	static FfxErrorCode executeGpuJobCompute(BackendContext_VK* backendContext, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {
		uint32_t               imageInfoIndex = 0;
		uint32_t               bufferInfoIndex = 0;
		uint32_t               descriptorWriteIndex = 0;
		uint32_t               dynamicOffsets[FSR2_MAX_UNIFORM_BUFFERS];
		VkDescriptorImageInfo  imageInfos[FSR2_MAX_IMAGE_VIEWS];
		VkDescriptorBufferInfo bufferInfos[FSR2_MAX_UNIFORM_BUFFERS];
		VkWriteDescriptorSet   writeDatas[FSR2_MAX_IMAGE_VIEWS + FSR2_MAX_UNIFORM_BUFFERS];

		FSR2Context* fsr2Context = backendContext->context;
		BackendContext_VK::PipelineLayout* pipelineLayout = reinterpret_cast<BackendContext_VK::PipelineLayout*>(job->computeJobDescriptor.pipeline.rootSignature);

		// bind uavs
		for (uint32_t uav = 0; uav < job->computeJobDescriptor.pipeline.uavCount; ++uav) {
			addBarrier(backendContext, &job->computeJobDescriptor.uavs[uav], FFX_RESOURCE_STATE_UNORDERED_ACCESS);

			const auto& fsr2Resource = fsr2Context->resources[job->computeJobDescriptor.uavs[uav].internalIndex];
			const auto image = fsr2Resource.image;

			writeDatas[descriptorWriteIndex] = {};
			writeDatas[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDatas[descriptorWriteIndex].dstSet = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
			writeDatas[descriptorWriteIndex].descriptorCount = 1;
			writeDatas[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDatas[descriptorWriteIndex].pImageInfo = &imageInfos[imageInfoIndex];
			writeDatas[descriptorWriteIndex].dstBinding = job->computeJobDescriptor.pipeline.uavResourceBindings[uav].slotIndex;
			writeDatas[descriptorWriteIndex].dstArrayElement = 0;

			imageInfos[imageInfoIndex] = {};
			imageInfos[imageInfoIndex].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfos[imageInfoIndex].imageView = image->mipMapViews.empty() ? image->view : image->mipMapViews[job->computeJobDescriptor.uavMip[uav]];

			AE_ASSERT(job->computeJobDescriptor.uavMip[uav] == 0 || !image->mipMapViews.empty());

			auto binding = job->computeJobDescriptor.pipeline.uavResourceBindings[uav].slotIndex;
			auto mipLevel = job->computeJobDescriptor.uavMip[uav];
			commandList->BindImage(image, 1, binding, image->mipMapViews.empty() ? -1 : mipLevel);

			imageInfoIndex++;
			descriptorWriteIndex++;
		}

		// bind srvs
		for (uint32_t srv = 0; srv < job->computeJobDescriptor.pipeline.srvCount; ++srv) {
			addBarrier(backendContext, &job->computeJobDescriptor.srvs[srv], FFX_RESOURCE_STATE_COMPUTE_READ);

			const auto& fsr2Resource = fsr2Context->resources[job->computeJobDescriptor.srvs[srv].internalIndex];
			const auto image = fsr2Resource.image;

			writeDatas[descriptorWriteIndex] = {};
			writeDatas[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDatas[descriptorWriteIndex].dstSet = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
			writeDatas[descriptorWriteIndex].descriptorCount = 1;
			writeDatas[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			writeDatas[descriptorWriteIndex].pImageInfo = &imageInfos[imageInfoIndex];
			writeDatas[descriptorWriteIndex].dstBinding = job->computeJobDescriptor.pipeline.srvResourceBindings[srv].slotIndex;
			writeDatas[descriptorWriteIndex].dstArrayElement = 0;

			imageInfos[imageInfoIndex] = {};
			imageInfos[imageInfoIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[imageInfoIndex].imageView = fsr2Resource.image->view;

			auto binding = job->computeJobDescriptor.pipeline.srvResourceBindings[srv].slotIndex;
			commandList->BindImage(image, 1, binding);

			imageInfoIndex++;
			descriptorWriteIndex++;
		}

		// update ubos
		for (uint32_t i = 0; i < job->computeJobDescriptor.pipeline.constCount; ++i) {
			writeDatas[descriptorWriteIndex] = {};
			writeDatas[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDatas[descriptorWriteIndex].dstSet = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
			writeDatas[descriptorWriteIndex].descriptorCount = 1;
			writeDatas[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			writeDatas[descriptorWriteIndex].pBufferInfo = &bufferInfos[bufferInfoIndex];
			writeDatas[descriptorWriteIndex].dstBinding = job->computeJobDescriptor.pipeline.cbResourceBindings[i].slotIndex;
			writeDatas[descriptorWriteIndex].dstArrayElement = 0;

			auto& buffer = fsr2Context->GetNextUniformBuffer();
			buffer.SetData(job->computeJobDescriptor.cbs[i].data, 0, job->computeJobDescriptor.cbs[i].uint32Size * sizeof(uint32_t));

			bufferInfos[bufferInfoIndex].offset = 0;
			bufferInfos[bufferInfoIndex].buffer = buffer.Get()->GetCurrent()->buffer;
			bufferInfos[bufferInfoIndex].range = job->computeJobDescriptor.cbs[i].uint32Size * sizeof(uint32_t);

			dynamicOffsets[bufferInfoIndex] = 0;

			auto binding = job->computeJobDescriptor.pipeline.cbResourceBindings[i].slotIndex;
			buffer.Bind(commandList, 1, binding);

			bufferInfoIndex++;
			descriptorWriteIndex++;
		}

		commandList->BindSampler(fsr2Context->pointSampler, 0, 0);
		commandList->BindSampler(fsr2Context->linearSampler, 0, 1);

		// insert all the barriers 
		flushBarriers(backendContext, commandList);

		// update all uavs and srvs
		// vkUpdateDescriptorSets(backendContext->device, descriptorWriteIndex, writeDatas, 0, nullptr);

		const auto& pipeline = fsr2Context->passes[pipelineLayout->passIndex].pipeline;
		commandList->BindPipeline(pipeline);

		// bind pipeline
		// vkCmdBindPipeline(commandList->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

		// bind descriptor sets 
		VkDescriptorSet sets[] = {
			backendContext->samplerDescriptorSet,
			pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex],
		};

		// vkCmdBindDescriptorSets(commandList->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, 0, 2, sets, bufferInfoIndex, dynamicOffsets);

		// dispatch
		//vkCmdDispatch(commandList->commandBuffer, job->computeJobDescriptor.dimensions[0], job->computeJobDescriptor.dimensions[1], job->computeJobDescriptor.dimensions[2]);
		commandList->Dispatch( job->computeJobDescriptor.dimensions[0], job->computeJobDescriptor.dimensions[1], job->computeJobDescriptor.dimensions[2]);

		// move to another descriptor set for the next compute render job so that we don't overwrite descriptors in-use
		pipelineLayout->descriptorSetIndex++;

		if (pipelineLayout->descriptorSetIndex >= FSR2_MAX_QUEUED_FRAMES)
			pipelineLayout->descriptorSetIndex = 0;

		return FFX_OK;
	}

	static FfxErrorCode executeGpuJobCopy(BackendContext_VK* backendContext, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {

		FSR2Context* fsr2Context = backendContext->context;
		const auto& fsr2ResourceSrc = fsr2Context->resources[job->copyJobDescriptor.src.internalIndex];
		const auto& fsr2ResourceDst = fsr2Context->resources[job->copyJobDescriptor.dst.internalIndex];

		addBarrier(backendContext, &job->copyJobDescriptor.src, FFX_RESOURCE_STATE_COPY_SRC);
		addBarrier(backendContext, &job->copyJobDescriptor.dst, FFX_RESOURCE_STATE_COPY_DEST);
		flushBarriers(backendContext, commandList);

		if (fsr2ResourceSrc.desc.type == FFX_RESOURCE_TYPE_BUFFER && fsr2ResourceDst.desc.type == FFX_RESOURCE_TYPE_BUFFER) {
			AE_ASSERT(false);
		}
		else if (fsr2ResourceSrc.desc.type == FFX_RESOURCE_TYPE_BUFFER && fsr2ResourceDst.desc.type != FFX_RESOURCE_TYPE_BUFFER) {
			AE_ASSERT(false);
		}
		else {
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

			vkCmdCopyImage(commandList->commandBuffer, vkResourceSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkResourceDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fsr2ResourceSrc.desc.mipCount, imageCopies);
		}

		return FFX_OK;
	}

	static FfxErrorCode executeGpuJobClearFloat(BackendContext_VK* backendContext, FfxGpuJobDescription* job, Graphics::CommandList* commandList) {
		uint32_t idx = job->clearJobDescriptor.target.internalIndex;

		FSR2Context* fsr2Context = backendContext->context;
		const auto& fsr2Resource = fsr2Context->resources[idx];

		if (fsr2Resource.desc.type != FFX_RESOURCE_TYPE_BUFFER) {
			addBarrier(backendContext, &job->clearJobDescriptor.target, FFX_RESOURCE_STATE_COPY_DEST);
			flushBarriers(backendContext, commandList);

			VkClearColorValue clearColor = {};

			clearColor.float32[0] = job->clearJobDescriptor.color[0];
			clearColor.float32[1] = job->clearJobDescriptor.color[1];
			clearColor.float32[2] = job->clearJobDescriptor.color[2];
			clearColor.float32[3] = job->clearJobDescriptor.color[3];

			commandList->ClearImageColor(fsr2Resource.image, clearColor);
		}

		return FFX_OK;
	}

	FfxErrorCode ExecuteGpuJobsVK(FfxFsr2Interface* backendInterface, FfxCommandList ffxCommandList) {
		AE_ASSERT(NULL != backendInterface);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

		FfxErrorCode errorCode = FFX_OK;

		// execute all renderjobs
		for (uint32_t i = 0; i < backendContext->gpuJobCount; ++i) {
			FfxGpuJobDescription* gpuJob = &backendContext->gpuJobs[i];
			Graphics::CommandList* commandList = reinterpret_cast<Graphics::CommandList*>(ffxCommandList);

			switch (gpuJob->jobType) {
			case FFX_GPU_JOB_CLEAR_FLOAT:
			{
				errorCode = executeGpuJobClearFloat(backendContext, gpuJob, commandList);
				break;
			}
			case FFX_GPU_JOB_COPY:
			{
				errorCode = executeGpuJobCopy(backendContext, gpuJob, commandList);
				break;
			}
			case FFX_GPU_JOB_COMPUTE:
			{
				errorCode = executeGpuJobCompute(backendContext, gpuJob, commandList);
				break;
			}
			default:;
			}
		}

		// check the execute function returned cleanly.
		FFX_RETURN_ON_ERROR(
			errorCode == FFX_OK,
			FFX_ERROR_BACKEND_API_ERROR);

		backendContext->gpuJobCount = 0;

		return FFX_OK;
	}

	FfxErrorCode DestroyResourceVK(FfxFsr2Interface* backendInterface, FfxResourceInternal resource) {
		AE_ASSERT(backendInterface != nullptr);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;

		if (resource.internalIndex != -1) {
			fsr2Context->RemoveResource(resource.internalIndex);
		}

		return FFX_OK;
	}

	static FfxErrorCode CreatePipeline(FfxFsr2Interface* backendInterface, FfxFsr2Pass pass, const FfxPipelineDescription* pipelineDescription, FfxPipelineState* outPipeline) {
		AE_ASSERT(NULL != backendInterface);
		AE_ASSERT(NULL != pipelineDescription);

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;

		auto& fsr2Pass = fsr2Context->passes[pass];

		// query device capabilities 
		FfxDeviceCapabilities deviceCapabilities;
		const uint32_t defaultSubgroupSize = 16;

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
		bool supportedFP16 = true;

		if (pass == FFX_FSR2_PASS_ACCUMULATE || pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN) {
			VkPhysicalDeviceProperties physicalDeviceProperties = {};
			vkGetPhysicalDeviceProperties(backendContext->physicalDevice, &physicalDeviceProperties);

			// Workaround: Disable FP16 path for the accumulate pass on NVIDIA due to reduced occupancy and high VRAM throughput.
			if (physicalDeviceProperties.vendorID == 0x10DE)
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

		// Some of their shaders don't support FP16 math
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

		AE_ASSERT(srvIndex == outPipeline->srvCount);
		AE_ASSERT(uavIndex == outPipeline->uavCount);
		AE_ASSERT(cbIndex == outPipeline->constCount);

		AE_ASSERT(backendContext->allocatedPipelineLayoutCount < FFX_FSR2_PASS_COUNT);
		BackendContext_VK::PipelineLayout& pipelineLayout = backendContext->pipelineLayouts[backendContext->allocatedPipelineLayoutCount++];

		// allocate descriptor sets
		pipelineLayout.descriptorSetIndex = 0;
		pipelineLayout.descriptorSetLayout = variant->sets[1].layout->layout;

		for (uint32_t i = 0; i < FSR2_MAX_QUEUED_FRAMES; i++) {
			VkDescriptorSetAllocateInfo allocateInfo = {};
			allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocateInfo.descriptorPool = fsr2Context->descriptorPool->GetNativePool();
			allocateInfo.descriptorSetCount = 1;
			allocateInfo.pSetLayouts = &pipelineLayout.descriptorSetLayout;

			vkAllocateDescriptorSets(backendContext->device, &allocateInfo, &pipelineLayout.descriptorSets[i]);
		}

		auto& shaderStageCreateInfo = variant->stageCreateInfos.front();

		// set wave64 if possible
		VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroupSizeCreateInfo = {};

		if (canForceWave64) {
			subgroupSizeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO;
			subgroupSizeCreateInfo.requiredSubgroupSize = 64;

			shaderStageCreateInfo.pNext = &subgroupSizeCreateInfo;
		}

		// variant->sets[0].layout->layout = backendContext->samplerDescriptorSetLayout;

		Graphics::ComputePipelineDesc pipelineDesc = {
			.shader = variant
		};
		fsr2Pass.pipeline = fsr2Context->device->CreatePipeline(pipelineDesc);

		pipelineLayout.passIndex = pass;
		pipelineLayout.pipelineLayout = fsr2Pass.pipeline->layout;

		outPipeline->pipeline = reinterpret_cast<FfxPipeline>(fsr2Pass.pipeline->pipeline);
		outPipeline->rootSignature = reinterpret_cast<FfxRootSignature>(&pipelineLayout);

		return FFX_OK;
	}

	static FfxErrorCode DestroyPipeline(FfxFsr2Interface* backendInterface, FfxPipelineState* pipeline) {

		return FFX_OK;

	}

	static FfxErrorCode CreateBackendContext(FfxFsr2Interface* backendInterface, FfxDevice device) {

		BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
		FSR2Context* fsr2Context = backendContext->context;

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

		jitter = 2.0f * jitter - 1.0f;

		return vec2(jitter.x / (float)target->GetScaledWidth(), jitter.y / (float)target->GetScaledHeight());

	}

	void FSR2Renderer::Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) {

		if (!initializationParameters.callbacks.scratchBuffer ||
			initializationParameters.displaySize.width != target->GetWidth() ||
			initializationParameters.displaySize.height != target->GetHeight() ||
			initializationParameters.maxRenderSize.width != target->GetScaledWidth() ||
			initializationParameters.maxRenderSize.height != target->GetScaledHeight()) {
			DestroyContext();
			CreateContext(target, device);
		}

		auto& postProcessing = scene->postProcessing;
		auto& sharpen = postProcessing.sharpen;
		auto& camera = scene->GetMainCamera();

		auto targetData = target->GetData(FULL_RES);

		auto colorImage = target->lightingTexture.image;
		auto velocityImage = target->GetVelocity()->image;
		auto depthImage = targetData->depthTexture->image;
		auto outputImage = target->GetLastHistory()->image;

		std::vector<Graphics::BufferBarrier> bufferBarriers;
		std::vector<Graphics::ImageBarrier> imageBarriers;
		imageBarriers.push_back({ outputImage,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_SHADER_WRITE_BIT });
		commandList->PipelineBarrier(imageBarriers, bufferBarriers);

		FfxFsr2DispatchDescription dispatchParameters = {};
		dispatchParameters.commandList = commandList;
		dispatchParameters.color = GetResource(&context, colorImage, L"FSR2_InputColor", FFX_RESOURCE_STATE_COMPUTE_READ);
		dispatchParameters.depth = GetResource(&context, depthImage, L"FSR2_InputDepth", FFX_RESOURCE_STATE_COMPUTE_READ);
		dispatchParameters.motionVectors = GetResource(&context, velocityImage, L"FSR2_InputMotionVectors", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.exposure = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_InputExposure", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.reactive = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyInputReactiveMap", FFX_RESOURCE_STATE_COMPUTE_READ);
		//dispatchParameters.transparencyAndComposition = GetTextureResource(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyTransparencyAndCompositionMap", FFX_RESOURCE_STATE_COMPUTE_READ);

		dispatchParameters.output = GetResource(&context, outputImage, L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
		dispatchParameters.jitterOffset.x = camera.GetJitter().x * float(target->GetScaledWidth()) + 1.0f;
		dispatchParameters.jitterOffset.y = camera.GetJitter().y * float(target->GetScaledHeight()) + 1.0f;
		dispatchParameters.motionVectorScale.x = float(target->GetScaledWidth());
		dispatchParameters.motionVectorScale.y = float(target->GetScaledHeight());
		dispatchParameters.reset = false;
		dispatchParameters.enableSharpening = false;
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

	}

	static void onFSR2Msg(FfxFsr2MsgType type, const wchar_t* message) {

		auto wstring = std::wstring(message);
		auto string = std::string(wstring.begin(), wstring.end());
		Log::Error(string);

	}

	void FSR2Renderer::CreateContext(const Ref<RenderTarget>& target, Graphics::GraphicsDevice* device) {

		const size_t contextSize = sizeof(FSR2Context);
		void* contextBuffer = malloc(contextSize);

		FSR2Context* fsr2Context = new FSR2Context();
		fsr2Context->device = device;

		initializationParameters.flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE;

		if (true) {
			initializationParameters.flags |= FFX_FSR2_ENABLE_DEBUG_CHECKING;
			initializationParameters.fpMessage = &onFSR2Msg;
		}

		initializationParameters.flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;

		for (int32_t i = 0; i < FFX_FSR2_PASS_COUNT; i++) {
			auto& pass = fsr2Context->passes[i];

			pass.shader = shaders[i];

		}

		const size_t scratchBufferSize = Fsr2GetScratchMemorySizeVK(device->physicalDevice);
		void* scratchBuffer = malloc(scratchBufferSize);
		FfxErrorCode errorCode = Fsr2GetInterfaceVK(&initializationParameters.callbacks, scratchBuffer, scratchBufferSize, fsr2Context, device, vkGetDeviceProcAddr);
		AE_ASSERT(errorCode == FFX_OK);

		initializationParameters.device = device;
		initializationParameters.displaySize.width = uint32_t(target->GetWidth());
		initializationParameters.displaySize.height = uint32_t(target->GetHeight());
		initializationParameters.maxRenderSize.width = uint32_t(target->GetScaledWidth());
		initializationParameters.maxRenderSize.height = uint32_t(target->GetScaledHeight());

		initializationParameters.callbacks.fpCreatePipeline = CreatePipeline;
		initializationParameters.callbacks.fpDestroyPipeline = DestroyPipeline;

		ffxFsr2ContextCreate(&context, &initializationParameters);

	}

	void FSR2Renderer::DestroyContext() {

		device->WaitForIdle();

		if (initializationParameters.callbacks.scratchBuffer != nullptr) {
			ffxFsr2ContextDestroy(&context);
			BackendContext_VK* backendContext = (BackendContext_VK*)initializationParameters.callbacks.scratchBuffer;
			delete backendContext->context;

			free(initializationParameters.callbacks.scratchBuffer);
			initializationParameters.callbacks.scratchBuffer = nullptr;
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