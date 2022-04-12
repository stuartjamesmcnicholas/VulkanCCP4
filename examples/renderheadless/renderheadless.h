#ifndef _CCP4_RENDER_PUSH_CONSTANTS_
#define _CCP4_RENDER_PUSH_CONSTANTS_

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "vulkanExample", __VA_ARGS__))
#else
#define LOG(...) printf(__VA_ARGS__)
#endif

struct PushConsts {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model;
    glm::mat4 depthBiasMVP;
    glm::vec4 lightPos;
    glm::vec4 fogColor;
    float fog_start;
    int _pad0;
    float fog_end;
    int _pad1;

} pushConsts;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType,
	uint64_t object,
	size_t location,
	int32_t messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	LOG("[VALIDATION]: %s - %s\n", pLayerPrefix, pMessage);
	return VK_FALSE;
}

class VulkanHeadless
{
public:
	vks::Buffer ubo;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	uint32_t queueFamilyIndex;
	VkPipelineCache pipelineCache;
	VkQueue queue;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkDescriptorSet descriptorSet;
        VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkPipeline pipeline_LINE;
	VkPipeline pipeline_TRIANGLES;
	std::vector<VkShaderModule> shaderModules;
	VkBuffer vertexBuffer, indexBuffer;
	VkDeviceMemory vertexMemory, indexMemory;

	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	};
	VkFramebuffer framebuffer;
	FrameBufferAttachment colorAttachment, depthAttachment;
	VkRenderPass renderPass;

	VkDebugReportCallbackEXT debugReportCallback{};

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
			if ((typeBits & 1) == 1) {
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
					return i;
				}
			}
			typeBits >>= 1;
		}
		return 0;
	}

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer *buffer, VkDeviceMemory *memory, VkDeviceSize size, void *data = nullptr)
	{
		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer));

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		vkGetBufferMemoryRequirements(device, *buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, memory));

		if (data != nullptr) {
			void *mapped;
			VK_CHECK_RESULT(vkMapMemory(device, *memory, 0, size, 0, &mapped));
			memcpy(mapped, data, size);
			vkUnmapMemory(device, *memory);
		}

		VK_CHECK_RESULT(vkBindBufferMemory(device, *buffer, *memory, 0));

		return VK_SUCCESS;
	}

	/*
		Submit command buffer to a queue and wait for fence until queue operations have been finished
	*/
	void submitWork(VkCommandBuffer cmdBuffer, VkQueue queue)
	{
		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
		VkFence fence;
		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &fence));
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
		VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
		vkDestroyFence(device, fence, nullptr);
	}

	VulkanHeadless(const std::string &filename, const std::string &outputFilename, const PushConsts &pc, int width, int height)
	{
		LOG("Running headless rendering example\n");

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		LOG("loading vulkan lib");
		vks::android::loadVulkanLibrary();
#endif

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan headless example";
		appInfo.pEngineName = "VulkanExampleHeadless";
		appInfo.apiVersion = VK_API_VERSION_1_0;

		/*
			Vulkan instance creation (without surface extensions)
		*/
		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;

		uint32_t layerCount = 0;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		const char* validationLayers[] = { "VK_LAYER_GOOGLE_threading",	"VK_LAYER_LUNARG_parameter_validation",	"VK_LAYER_LUNARG_object_tracker","VK_LAYER_LUNARG_core_validation",	"VK_LAYER_LUNARG_swapchain", "VK_LAYER_GOOGLE_unique_objects" };
		layerCount = 6;
#else
		const char* validationLayers[] = { "VK_LAYER_LUNARG_standard_validation" };
		layerCount = 1;
#endif
#if DEBUG
		// Check if layers are available
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());

		bool layersAvailable = true;
		for (auto layerName : validationLayers) {
			bool layerAvailable = false;
			for (auto instanceLayer : instanceLayers) {
				if (strcmp(instanceLayer.layerName, layerName) == 0) {
					layerAvailable = true;
					break;
				}
			}
			if (!layerAvailable) {
				layersAvailable = false;
				break;
			}
		}

		const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
		if (layersAvailable) {
			instanceCreateInfo.ppEnabledLayerNames = validationLayers;
			instanceCreateInfo.enabledLayerCount = layerCount;
			instanceCreateInfo.enabledExtensionCount = 1;
			instanceCreateInfo.ppEnabledExtensionNames = &validationExt;
		}
#endif
		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		vks::android::loadVulkanFunctions(instance);
#endif
#if DEBUG
		if (layersAvailable) {
			VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {};
			debugReportCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debugReportCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
			debugReportCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugMessageCallback;

			// We have to explicitly load this function.
			PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
			assert(vkCreateDebugReportCallbackEXT);
			VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(instance, &debugReportCreateInfo, nullptr, &debugReportCallback));
		}
#endif

		/*
			Vulkan device creation
		*/
		uint32_t deviceCount = 0;
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data()));
		physicalDevice = physicalDevices[0];

                VkPhysicalDeviceProperties pProperties;
                vkGetPhysicalDeviceProperties(physicalDevice,&pProperties);
                std::cout << "Max image dimension " << pProperties.limits.maxImageDimension2D << std::endl;
                if(width>pProperties.limits.maxImageDimension2D) width = pProperties.limits.maxImageDimension2D;
                if(height>pProperties.limits.maxImageDimension2D) height = pProperties.limits.maxImageDimension2D;

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		LOG("GPU: %s\n", deviceProperties.deviceName);

		// Request a single graphics queue
		const float defaultQueuePriority(0.0f);
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				queueFamilyIndex = i;
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = i;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &defaultQueuePriority;
				break;
			}
		}
		// Create logical device
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

		// Get a graphics queue
		vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

		// Command pool
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool));

		/*
			Prepare vertex and index buffers
		*/
		struct Vertex {
                glm::vec3 pos;
                glm::vec3 normal;
                glm::vec2 uv;
                glm::vec4 color;
                glm::vec4 joint0;
                glm::vec4 weight0;
                glm::vec4 tangent;
		};
		{
			std::vector<Vertex> vertices = {
				{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
				{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
			};
			std::vector<uint32_t> indices = { 0, 1, 2 };

			const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
			const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			// Command buffer for copy commands (reused)
			VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VkCommandBuffer copyCmd;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &copyCmd));
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

			// Copy input data to VRAM using a staging buffer
			{
				// Vertices
				createBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&stagingBuffer,
					&stagingMemory,
					vertexBufferSize,
					vertices.data());

				createBuffer(
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					&vertexBuffer,
					&vertexMemory,
					vertexBufferSize);

				VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
				VkBufferCopy copyRegion = {};
				copyRegion.size = vertexBufferSize;
				vkCmdCopyBuffer(copyCmd, stagingBuffer, vertexBuffer, 1, &copyRegion);
				VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

				submitWork(copyCmd, queue);

				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingMemory, nullptr);

				// Indices
				createBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					&stagingBuffer,
					&stagingMemory,
					indexBufferSize,
					indices.data());

				createBuffer(
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					&indexBuffer,
					&indexMemory,
					indexBufferSize);

				VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
				copyRegion.size = indexBufferSize;
				vkCmdCopyBuffer(copyCmd, stagingBuffer, indexBuffer, 1, &copyRegion);
				VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

				submitWork(copyCmd, queue);

				vkDestroyBuffer(device, stagingBuffer, nullptr);
				vkFreeMemory(device, stagingMemory, nullptr);
			}
		}

		/*
			Create framebuffer attachments
		*/
		VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
		VkFormat depthFormat;
		vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
		{
			// Color attachment
			VkImageCreateInfo image = vks::initializers::imageCreateInfo();
			image.imageType = VK_IMAGE_TYPE_2D;
			image.format = colorFormat;
			image.extent.width = width;
			image.extent.height = height;
			image.extent.depth = 1;
			image.mipLevels = 1;
			image.arrayLayers = 1;
			image.samples = VK_SAMPLE_COUNT_1_BIT;
			image.tiling = VK_IMAGE_TILING_OPTIMAL;
			image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;

			VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &colorAttachment.image));
			vkGetImageMemoryRequirements(device, colorAttachment.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &colorAttachment.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, colorAttachment.image, colorAttachment.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = colorFormat;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = colorAttachment.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &colorAttachment.view));

			// Depth stencil attachment
			image.format = depthFormat;
			image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthAttachment.image));
			vkGetImageMemoryRequirements(device, depthAttachment.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &depthAttachment.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, depthAttachment.image, depthAttachment.memory, 0));

			VkImageViewCreateInfo depthStencilView = vks::initializers::imageViewCreateInfo();
			depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			depthStencilView.format = depthFormat;
			depthStencilView.flags = 0;
			depthStencilView.subresourceRange = {};
			depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			depthStencilView.subresourceRange.baseMipLevel = 0;
			depthStencilView.subresourceRange.levelCount = 1;
			depthStencilView.subresourceRange.baseArrayLayer = 0;
			depthStencilView.subresourceRange.layerCount = 1;
			depthStencilView.image = depthAttachment.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthAttachment.view));
		}

		/*
			Create renderpass
		*/
		{
			std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
			// Color attachment
			attchmentDescriptions[0].format = colorFormat;
			attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			// Depth attachment
			attchmentDescriptions[1].format = depthFormat;
			attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;
			subpassDescription.pDepthStencilAttachment = &depthReference;

			// Use subpass dependencies for layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// Create the actual renderpass
			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
			renderPassInfo.pAttachments = attchmentDescriptions.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

			VkImageView attachments[2];
			attachments[0] = colorAttachment.view;
			attachments[1] = depthAttachment.view;

			VkFramebufferCreateInfo framebufferCreateInfo = vks::initializers::framebufferCreateInfo();
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = 2;
			framebufferCreateInfo.pAttachments = attachments;
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer));
		}

                vks::VulkanDevice *dev = new vks::VulkanDevice(physicalDevice);
                dev->logicalDevice = device;
                dev->commandPool = commandPool;
		/*
			Prepare graphics pipeline
		*/
		{
                        VK_CHECK_RESULT(dev->createBuffer(
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &ubo,
                                    sizeof(pc)));
                        VK_CHECK_RESULT(ubo.map());

			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
                            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT|VK_SHADER_STAGE_VERTEX_BIT, 0),
                        };
			VkDescriptorSetLayoutCreateInfo descriptorLayout =
                            vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
                        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

                        std::vector<VkDescriptorPoolSize> poolSizes = {
                            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3),
                        };
                        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
                        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

                        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
                        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
                        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

/*
			// MVP via push constant block
			VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
*/
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

			// Create pipeline
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_TRIANGLES =
				vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
				vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, VK_FALSE);

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_LINE =
				vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, 0, VK_FALSE);

			VkPipelineRasterizationStateCreateInfo rasterizationState =
				vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

			VkPipelineColorBlendAttachmentState blendAttachmentState =
				vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

			VkPipelineColorBlendStateCreateInfo colorBlendState =
				vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

			VkPipelineDepthStencilStateCreateInfo depthStencilState =
				vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

			VkPipelineViewportStateCreateInfo viewportState =
				vks::initializers::pipelineViewportStateCreateInfo(1, 1);

			VkPipelineMultisampleStateCreateInfo multisampleState =
				vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

			std::vector<VkDynamicState> dynamicStateEnables = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
			};
			VkPipelineDynamicStateCreateInfo dynamicState =
				vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

			VkGraphicsPipelineCreateInfo pipelineCreateInfo =
				vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

			std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
                        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages_LINE{};

			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineCreateInfo.pRasterizationState = &rasterizationState;
			pipelineCreateInfo.pColorBlendState = &colorBlendState;
			pipelineCreateInfo.pMultisampleState = &multisampleState;
			pipelineCreateInfo.pViewportState = &viewportState;
			pipelineCreateInfo.pDepthStencilState = &depthStencilState;
			pipelineCreateInfo.pDynamicState = &dynamicState;
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineCreateInfo.pStages = shaderStages.data();

                        VkGraphicsPipelineCreateInfo pipelineCI_LINE = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
                        pipelineCI_LINE.pInputAssemblyState = &inputAssemblyState_LINE;
                        pipelineCI_LINE.pRasterizationState = &rasterizationState;
                        pipelineCI_LINE.pColorBlendState = &colorBlendState;
                        pipelineCI_LINE.pMultisampleState = &multisampleState;
                        pipelineCI_LINE.pViewportState = &viewportState;
                        pipelineCI_LINE.pDepthStencilState = &depthStencilState;
                        pipelineCI_LINE.pDynamicState = &dynamicState;
                        pipelineCI_LINE.stageCount = shaderStages_LINE.size();
                        pipelineCI_LINE.pStages = shaderStages_LINE.data();

                        VkGraphicsPipelineCreateInfo pipelineCI_TRIANGLES = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
                        pipelineCI_TRIANGLES.pInputAssemblyState = &inputAssemblyState_TRIANGLES;
                        pipelineCI_TRIANGLES.pRasterizationState = &rasterizationState;
                        pipelineCI_TRIANGLES.pColorBlendState = &colorBlendState;
                        pipelineCI_TRIANGLES.pMultisampleState = &multisampleState;
                        pipelineCI_TRIANGLES.pViewportState = &viewportState;
                        pipelineCI_TRIANGLES.pDepthStencilState = &depthStencilState;
                        pipelineCI_TRIANGLES.pDynamicState = &dynamicState;
                        pipelineCI_TRIANGLES.stageCount = shaderStages.size();
                        pipelineCI_TRIANGLES.pStages = shaderStages.data();

			// Vertex bindings an attributes
			// Binding description
			std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
				vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			};

			// Attribute descriptions
			std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
				vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),					// Position
				vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)),	// Normal
				vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)),	// Color
			};

			VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
			vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
			vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
			vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
			vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

			pipelineCreateInfo.pVertexInputState = &vertexInputState;
			pipelineCI_LINE.pVertexInputState = &vertexInputState;
			pipelineCI_TRIANGLES.pVertexInputState = &vertexInputState;

			// TODO: There is no command line arguments parsing (nor Android settings) for this
			// example, so we have no way of picking between GLSL or HLSL shaders.
			// Hard-code to glsl for now.
			const std::string shadersPath = getAssetPath() + "shaders/glsl/renderheadless/";

			shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderStages[0].pName = "main";
			shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderStages[1].pName = "main";
			shaderStages_LINE[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages_LINE[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			shaderStages_LINE[0].pName = "main";
			shaderStages_LINE[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages_LINE[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			shaderStages_LINE[1].pName = "main";
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
			shaderStages[0].module = vks::tools::loadShader(androidapp->activity->assetManager, (shadersPath + "triangle.vert.spv").c_str(), device);
			shaderStages[1].module = vks::tools::loadShader(androidapp->activity->assetManager, (shadersPath + "triangle.frag.spv").c_str(), device);
#else
			shaderStages[0].module = vks::tools::loadShader((shadersPath + "triangle.vert.spv").c_str(), device);
			shaderStages[1].module = vks::tools::loadShader((shadersPath + "triangle.frag.spv").c_str(), device);
			shaderStages_LINE[0].module = vks::tools::loadShader((shadersPath + "triangle.vert.spv").c_str(), device);
			shaderStages_LINE[1].module = vks::tools::loadShader((shadersPath + "scene-lines.frag.spv").c_str(), device);
#endif
			shaderModules = { shaderStages[0].module, shaderStages[1].module, shaderStages_LINE[0].module, shaderStages_LINE[1].module};
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI_LINE, nullptr, &pipeline_LINE));
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI_TRIANGLES, nullptr, &pipeline_TRIANGLES));

                        std::vector<VkWriteDescriptorSet> writeDescriptorSets;
                        writeDescriptorSets = {
                            // Binding 0 : Vertex shader uniform buffer
                            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor),
                        };
                        vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

		}

		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;

                vkglTF::Model scene;
		scene.loadFromFile(filename, dev, queue, glTFLoadingFlags);

		/*
			Command buffer creation
		*/
		{
			VkCommandBuffer commandBuffer;
			VkCommandBufferAllocateInfo cmdBufAllocateInfo =
				vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer));

			VkCommandBufferBeginInfo cmdBufInfo =
				vks::initializers::commandBufferBeginInfo();

			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

			VkClearValue clearValues[2];
			clearValues[0].color = { { pc.fogColor[0], pc.fogColor[1], pc.fogColor[2], pc.fogColor[3] } };
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffer;

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float)0.0f;
			viewport.maxDepth = (float)1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Update dynamic scissor state
			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			// Render scene
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                        //vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &(pc));
                        memcpy(ubo.mapped, &pc, sizeof(pc));

                        scene.draw(commandBuffer,0,0,1,TINYGLTF_MODE_TRIANGLE_STRIP);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_TRIANGLES);
                        scene.draw(commandBuffer,0,0,1,TINYGLTF_MODE_TRIANGLES);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_LINE);
                        scene.draw(commandBuffer,0,0,1,TINYGLTF_MODE_LINE);

			vkCmdEndRenderPass(commandBuffer);

			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

			submitWork(commandBuffer, queue);

			vkDeviceWaitIdle(device);
		}

		/*
			Copy framebuffer image to host visible image
		*/
		const char* imagedata;
		{
			// Create the linear tiled destination image to copy to and to read the memory from
			VkImageCreateInfo imgCreateInfo(vks::initializers::imageCreateInfo());
			imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imgCreateInfo.extent.width = width;
			imgCreateInfo.extent.height = height;
			imgCreateInfo.extent.depth = 1;
			imgCreateInfo.arrayLayers = 1;
			imgCreateInfo.mipLevels = 1;
			imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
			imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			// Create the image
			VkImage dstImage;
			VK_CHECK_RESULT(vkCreateImage(device, &imgCreateInfo, nullptr, &dstImage));
			// Create memory to back up the image
			VkMemoryRequirements memRequirements;
			VkMemoryAllocateInfo memAllocInfo(vks::initializers::memoryAllocateInfo());
			VkDeviceMemory dstImageMemory;
			vkGetImageMemoryRequirements(device, dstImage, &memRequirements);
			memAllocInfo.allocationSize = memRequirements.size;
			// Memory must be host visible to copy from
			memAllocInfo.memoryTypeIndex = getMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImageMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device, dstImage, dstImageMemory, 0));

			// Do the actual blit from the offscreen image to our host visible destination image
			VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
			VkCommandBuffer copyCmd;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &copyCmd));
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
			VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));

			// Transition destination image to transfer destination layout
			vks::tools::insertImageMemoryBarrier(
				copyCmd,
				dstImage,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			// colorAttachment.image is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

			VkImageCopy imageCopyRegion{};
			imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.srcSubresource.layerCount = 1;
			imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageCopyRegion.dstSubresource.layerCount = 1;
			imageCopyRegion.extent.width = width;
			imageCopyRegion.extent.height = height;
			imageCopyRegion.extent.depth = 1;

			vkCmdCopyImage(
				copyCmd,
				colorAttachment.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageCopyRegion);

			// Transition destination image to general layout, which is the required layout for mapping the image memory later on
			vks::tools::insertImageMemoryBarrier(
				copyCmd,
				dstImage,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_MEMORY_READ_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

			submitWork(copyCmd, queue);

			// Get layout of the image (including row pitch)
			VkImageSubresource subResource{};
			subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			VkSubresourceLayout subResourceLayout;

			vkGetImageSubresourceLayout(device, dstImage, &subResource, &subResourceLayout);

			// Map image memory so we can start copying from it
			vkMapMemory(device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imagedata);
			imagedata += subResourceLayout.offset;

		/*
			Save host visible framebuffer image to disk (ppm format)
		*/

                        std::string fname = "";
#if defined (VK_USE_PLATFORM_ANDROID_KHR)
                        fname += getenv("EXTERNAL_STORAGE");
                        fname += "/";
#endif
                        fname += outputFilename;
			const char* filename = fname.c_str();
			std::ofstream file(filename, std::ios::out | std::ios::binary);

			// ppm header
			file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";

			// If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
			// Check if source is BGR and needs swizzle
			std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
			const bool colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), VK_FORMAT_R8G8B8A8_UNORM) != formatsBGR.end());

			// ppm binary pixel data
			for (int32_t y = 0; y < height; y++) {
				unsigned int *row = (unsigned int*)imagedata;
				for (int32_t x = 0; x < width; x++) {
					if (colorSwizzle) {
						file.write((char*)row + 2, 1);
						file.write((char*)row + 1, 1);
						file.write((char*)row, 1);
					}
					else {
						file.write((char*)row, 3);
					}
					row++;
				}
				imagedata += subResourceLayout.rowPitch;
			}
			file.close();

			LOG("Framebuffer image saved to %s\n", filename);

			// Clean up resources
			vkUnmapMemory(device, dstImageMemory);
			vkFreeMemory(device, dstImageMemory, nullptr);
			vkDestroyImage(device, dstImage, nullptr);
		}

		vkQueueWaitIdle(queue);
	}

	~VulkanHeadless()
	{
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexMemory, nullptr);
		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexMemory, nullptr);
		vkDestroyImageView(device, colorAttachment.view, nullptr);
		vkDestroyImage(device, colorAttachment.image, nullptr);
		vkFreeMemory(device, colorAttachment.memory, nullptr);
		vkDestroyImageView(device, depthAttachment.view, nullptr);
		vkDestroyImage(device, depthAttachment.image, nullptr);
		vkFreeMemory(device, depthAttachment.memory, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipeline(device, pipeline_LINE, nullptr);
		vkDestroyPipeline(device, pipeline_TRIANGLES, nullptr);
		vkDestroyPipelineCache(device, pipelineCache, nullptr);
		vkDestroyCommandPool(device, commandPool, nullptr);
		for (auto shadermodule : shaderModules) {
			vkDestroyShaderModule(device, shadermodule, nullptr);
		}
		vkDestroyDevice(device, nullptr);
#if DEBUG
		if (debugReportCallback) {
			PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));
			assert(vkDestroyDebugReportCallback);
			vkDestroyDebugReportCallback(instance, debugReportCallback, nullptr);
		}
#endif
		vkDestroyInstance(instance, nullptr);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		vks::android::freeVulkanLibrary();
#endif
	}
};
#endif /* _CCP4_RENDER_PUSH_CONSTANTS_ */
