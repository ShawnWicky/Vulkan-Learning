#include "model.hpp"
#include "fullScreenPass.hpp"

namespace
{
	namespace cfg
	{
		// Compiled shader code for the graphics pipeline(s)
		// See sources in cw3/shaders/*. 
#		define SHADERDIR_ "assets/cw3/shaders/"
		constexpr char const* kVertShaderPath = SHADERDIR_ "PBR.vert.spv";
		constexpr char const* kFragShaderPath = SHADERDIR_ "PBR.frag.spv";

		constexpr char const* kfirstPassVertPath = SHADERDIR_ "MRT.vert.spv";
		constexpr char const* kfirstPassFragPath = SHADERDIR_ "MRT.frag.spv";
#		undef SHADERDIR_

#		define ASSETDIR_ "assets/cw3/"
		constexpr char const* kNewShipPath = ASSETDIR_ "NewShip.obj";
		constexpr char const* kNewMaterialtestPath = ASSETDIR_ "materialtest.obj";
#		undef ASSETDIR_
		// General rule: with a standard 24 bit or 32 bit float depth buffer,
		// you can support a 1:1000 ratio between the near and far plane with
		// minimal depth fighting. Larger ratios will introduce more depth
		// fighting problems; smaller ratios will increase the depth buffer's
		// resolution but will also limit the view distance.
		constexpr float kCameraNear  = 0.1f;
		constexpr float kCameraFar   = 100.f;

		constexpr auto kCameraFov    = 60.0_degf;

		constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;
		constexpr VkFormat kNormalFormat = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		constexpr VkFormat kAlbedoFormat = VK_FORMAT_R8G8B8A8_SRGB;
		constexpr VkFormat kPositionFormat = VK_FORMAT_R8G8B8A8_SRGB;
		//For Camera
		bool firstMouse = true;
		bool enableMouse = false;
		float lastX;
		float lastY;
		float last = 0.f;
		float delta;
		//For question 2.4
		bool moveable = false;
	}

	// Local types/structures:
	///--------------------------------------------------
	/// Camera Class
	///-------------------------------------------------
	//The camera class is based on learnOpenGL: https://learnopengl.com/Getting-started/Camera
	class Camera
	{
	public:
		//Ruglar
		Camera(glm::vec3 position, glm::vec3 target, glm::vec3 worldUp);

		Camera(glm::vec3 position, float pitch, float yaw, glm::vec3 worldUp);

		glm::vec3 position; //camera pos
		glm::vec3 forward; //camera forward direction
		glm::vec3 right; // camera right
		glm::vec3 up; // camera up
		glm::vec3 worldUp; //world up

		float yaw;
		float pitch;
		float speedX;
		float speedY;
		float speedZ;
		float speedScalar = 0.5f;

		glm::mat4 getViewMatrix();

		void updateCameraPosition();

		void updateCameraAngle();

		void processMouseMovement(float deltaX, float deltaY);
	};

	Camera camera = Camera(glm::vec3(0.f, -6.f, -15.f), glm::radians(0.f), glm::radians(0.f), glm::vec3(0.f, 1.f, 0.f));

	///-----------------------------------------------------------------------
	/// Uniform buffer
	///-----------------------------------------------------------------------

	namespace glsl
	{
		struct Light
		{
			glm::vec4 position;
			glm::vec4 colour;
		};

		struct SceneUniform
		{
			glm::mat4 camera;
			glm::mat4 projection;
			glm::mat4 projCam;
			Light lights[4];
			alignas(16)	glm::vec3 camPos;
			int constant;
		};

		static_assert(sizeof(SceneUniform) <= 65536, "SceneUniform must be less than 65536 bytes for vkCmdUpdateBuffer");
		static_assert(sizeof(SceneUniform) % 4 == 0, "SceneUniform size must be a multiple of 4 bytes");

		struct PBRuniform
		{
			// Note: must map to the std140 uniform interface in the fragment
			// shader, so need to be careful about the packing/alignment here!
			glm::vec4 emissive;
			glm::vec4 albedo;
			float shininess;
			float metalness;
		};
	}

	///-----------------------------------------------------------------------
	/// Load Obj files
	///-----------------------------------------------------------------------
	ModelData newShip = load_obj_model(cfg::kNewShipPath);

	// Local functions:

	// GLFW callbacks
	void glfw_callback_key_press(GLFWwindow*, int, int, int, int);

	void glfw_callback_cursor_pos(GLFWwindow* window, double xPos, double yPos);

	void glfw_callback_mouse_button(GLFWwindow* window, int, int, int);

	// Helpers:

	lut::DescriptorSetLayout create_second_render_pass_layout(lut::VulkanWindow const&);
	
	lut::PipelineLayout create_second_pipeline_layout(lut::VulkanContext const&, VkDescriptorSetLayout, VkDescriptorSetLayout);
	//second render pass
	lut::RenderPass create_second_render_pass(lut::VulkanWindow const&);
	lut::Pipeline create_second_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	
	std::tuple<lut::Image, lut::ImageView> create_depth_buffer(lut::VulkanWindow const&, lut::Allocator const&);

	//final framebuffer
	void create_swapchain_framebuffers(
		lut::VulkanWindow const&,
		VkRenderPass,
		std::vector<lut::Framebuffer>&,
		VkImageView aDepthView
	);

	//temporary framebuffer
	void update_scene_uniforms(
		Camera& camera,
		glsl::SceneUniform&,
		std::uint32_t aFramebufferWidth,
		std::uint32_t aFramebufferHeight
	);

	void record_commands(
		VkCommandBuffer,
		VkRenderPass,
		VkRenderPass,
		VkFramebuffer,
		VkFramebuffer,
		VkPipeline,
		VkExtent2D const&,
		//--------------------------------------
		ColourMesh&,

		VkBuffer aSceneUBO,
		std::vector<lut::Buffer>&,

		glsl::SceneUniform const&,

		VkPipelineLayout,
		VkDescriptorSet aSceneDescriptors,
		std::vector<VkDescriptorSet> aPBRDescriptors
		//--------------------------------------
	);
	
	void submit_commands(
		lut::VulkanWindow const&,
		VkCommandBuffer,
		VkFence,
		VkSemaphore,
		VkSemaphore
	);
	void present_results(
		VkQueue,
		VkSwapchainKHR,
		std::uint32_t aImageIndex,
		VkSemaphore,
		bool& aNeedToRecreateSwapchain
	);
}

int main() try
{
	/// <summary>
	/// initialize the light array
	/// </summary>
	glsl::SceneUniform sceneUniforms{};

	sceneUniforms.lights[0].position = glm::vec4(0.f, 9.3f, -3.f, 1.f);
	sceneUniforms.lights[0].colour = glm::vec4(1.f, 1.f, 0.8f, 1.f);

	sceneUniforms.lights[1].position = glm::vec4(3.f, 9.3f, -3.f, 1.f);
	sceneUniforms.lights[1].colour = glm::vec4(1.f, 0.f, 0.0f, 1.f);

	sceneUniforms.lights[2].position = glm::vec4(-3.f, 9.3f, -3.f, 1.f);
	sceneUniforms.lights[2].colour = glm::vec4(0.f, 1.f, 0.0f, 1.f);

	sceneUniforms.lights[3].position = glm::vec4(0.f, 9.3f, +3.f, 1.f);
	sceneUniforms.lights[3].colour = glm::vec4(0.f, 0.f, 1.0f, 1.f);

	// Create Vulkan Window
	auto window = lut::make_vulkan_window();

	glfwSetWindowUserPointer(window.window, &sceneUniforms);
	//Set the input Mode
	glfwSetInputMode(window.window, GLFW_CURSOR, NULL);

	// Configure the GLFW window
	glfwSetKeyCallback(window.window, &glfw_callback_key_press);

	glfwSetMouseButtonCallback(window.window, &glfw_callback_mouse_button);

	glfwSetCursorPosCallback(window.window, &glfw_callback_cursor_pos);

	// Create VMA allocator
	lut::Allocator allocator = lut::create_allocator(window);

	// Intialize resources
	lut::RenderPass renderPass = create_second_render_pass(window);
	lut::RenderPass fullscreen_renderPass = create_fullscreen_render_pass(window, cfg::kPositionFormat, cfg::kNormalFormat, cfg::kAlbedoFormat, cfg::kDepthFormat);

	//create scene descriptor set layout
	//call create_scene_descriptor_layout
	lut::DescriptorSetLayout sceneLayout = create_scene_descriptor_layout(window);
	lut::DescriptorSetLayout advancedLayout = create_advanced_descriptor_layout(window);
	lut::DescriptorSetLayout secondPassLayout = create_second_render_pass_layout(window);
	//create pipeline layout
	lut::PipelineLayout pipeLayout = create_fullscreen_pipeline_layout(window, sceneLayout.handle, advancedLayout.handle);
	lut::PipelineLayout secondPipeLayout = create_second_pipeline_layout(window, sceneLayout.handle, secondPassLayout.handle);
	lut::Pipeline pbrPipe = create_fullscreen_pipeline(window, fullscreen_renderPass.handle, pipeLayout.handle, cfg::kfirstPassVertPath, cfg::kfirstPassFragPath);
	lut::Pipeline sceondPipe = create_second_pipeline(window, renderPass.handle, secondPipeLayout.handle);

	//create depth buffer
	auto [depthBuffer, depthBufferView] = create_depth_buffer(window, allocator);

	//swapchain framebuffers
	std::vector<lut::Framebuffer> framebuffers;
	create_swapchain_framebuffers(window, renderPass.handle, framebuffers, depthBufferView.handle);

	lut::CommandPool cpool = lut::create_command_pool(window, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	std::vector<VkCommandBuffer> cbuffers;
	std::vector<lut::Fence> cbfences;

	for (std::size_t i = 0; i < framebuffers.size(); ++i)
	{
		cbuffers.emplace_back(lut::alloc_command_buffer(window, cpool.handle));
		cbfences.emplace_back(lut::create_fence(window, VK_FENCE_CREATE_SIGNALED_BIT));
	}

	lut::Semaphore imageAvailable = lut::create_semaphore(window);
	lut::Semaphore renderFinished = lut::create_semaphore(window);

	/// <summary>
	/// material buffer
	/// </summary>
	/// <returns></returns>
	ColourMesh materialMesh = createObjBuffer(newShip, window, allocator);

	//create descriptor pool
	lut::DescriptorPool dpool = lut::create_descriptor_pool(window);

	lut::Sampler defaultSampler = lut::create_default_sampler(window);
	lut::Sampler colourSampler = lut::create_colour_sampler(window);
#pragma region secene uniform, light and ambient buffer (with thier descriptorSets)
	// create scene uniform buffer with lut::create_buffer()
	lut::Buffer sceneUBO = lut::create_buffer(
		allocator,
		sizeof(glsl::SceneUniform),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);
	// allocate descriptor set for uniform buffer
	VkDescriptorSet sceneDescriptors = lut::alloc_desc_set(window, dpool.handle, sceneLayout.handle);
	{
		VkWriteDescriptorSet desc[1]{};
		VkDescriptorBufferInfo sceneUboInfo{};
		sceneUboInfo.buffer = sceneUBO.buffer;
		sceneUboInfo.range = VK_WHOLE_SIZE;

		desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[0].dstSet = sceneDescriptors;
		desc[0].dstBinding = 0;
		desc[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		desc[0].descriptorCount = 1;
		desc[0].pBufferInfo = &sceneUboInfo;

		//initialize descriptor set with vkUpdateDescriptorSets
		constexpr auto numSets = sizeof(desc) / sizeof(desc[0]);
		vkUpdateDescriptorSets(window.device, numSets, desc, 0, nullptr);
	}
#pragma endregion

#pragma region material uniform buffers and descriptors
	std::vector<lut::Buffer> pbrBuffers(newShip.materials.size());
	std::vector<VkDescriptorSet> pbrDescriptors(newShip.materials.size());
	for (int i = 0; i < newShip.materials.size(); i++)
	{
		pbrBuffers[i] = lut::create_buffer(
			allocator,
			sizeof(glsl::PBRuniform),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY
		);

		pbrDescriptors[i] = lut::alloc_desc_set(window, dpool.handle, advancedLayout.handle);
		{
			VkWriteDescriptorSet desc[1]{};
			VkDescriptorBufferInfo materialInfo{};
			materialInfo.buffer = pbrBuffers[i].buffer;
			materialInfo.range = VK_WHOLE_SIZE;

			desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			desc[0].dstSet = pbrDescriptors[i];
			desc[0].dstBinding = 0;
			desc[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			desc[0].descriptorCount = 1;
			desc[0].pBufferInfo = &materialInfo;

			constexpr auto numSets = sizeof(desc) / sizeof(desc[0]);
			vkUpdateDescriptorSets(window.device, numSets, desc, 0, nullptr);
		}
	}
#pragma endregion

	lut::Image position_image = lut::create_image(allocator, window.swapchainExtent.width, window.swapchainExtent.height, cfg::kPositionFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	lut::ImageView position_view = lut::create_image_view(window, position_image.image, cfg::kPositionFormat);

	lut::Image normal_image = lut::create_image(allocator, window.swapchainExtent.width, window.swapchainExtent.height, cfg::kNormalFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	lut::ImageView normal_view = lut::create_image_view(window, normal_image.image, cfg::kNormalFormat);

	lut::Image albedo_image = lut::create_image(allocator, window.swapchainExtent.width, window.swapchainExtent.height, cfg::kAlbedoFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	lut::ImageView albedo_view = lut::create_image_view(window, albedo_image.image, cfg::kAlbedoFormat);

	lut::Image emissive_image = lut::create_image(allocator, window.swapchainExtent.width, window.swapchainExtent.height, cfg::kAlbedoFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	lut::ImageView emissive_view = lut::create_image_view(window, emissive_image.image, cfg::kAlbedoFormat);
	//fullscreen framebuffers
	std::vector<lut::Framebuffer> offscreen_framebuffers;
	create_fullscreen_framebuffers(allocator, window, fullscreen_renderPass.handle, offscreen_framebuffers, position_view.handle, normal_view.handle, albedo_view.handle, emissive_view.handle, depthBufferView.handle);

	VkDescriptorSet textureSet = lut::alloc_desc_set(window, dpool.handle, secondPassLayout.handle);
	{
		VkWriteDescriptorSet desc[4]{};
		VkDescriptorImageInfo positionInfo{};
		positionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		positionInfo.imageView = position_view.handle;
		positionInfo.sampler = defaultSampler.handle;

		VkDescriptorImageInfo normalInfo{};
		normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalInfo.imageView = normal_view.handle;
		normalInfo.sampler = defaultSampler.handle;

		VkDescriptorImageInfo albedoInfo{};
		albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		albedoInfo.imageView = albedo_view.handle;
		albedoInfo.sampler = colourSampler.handle;

		VkDescriptorImageInfo emissiveInfo{};
		albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		albedoInfo.imageView = emissive_view.handle;
		albedoInfo.sampler = colourSampler.handle;

		desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[0].dstSet = textureSet;
		desc[0].dstBinding = 0;
		desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc[0].descriptorCount = 1;
		desc[0].pImageInfo = &positionInfo;

		desc[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[1].dstSet = textureSet;
		desc[1].dstBinding = 1;
		desc[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc[1].descriptorCount = 1;
		desc[1].pImageInfo = &normalInfo;

		desc[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[2].dstSet = textureSet;
		desc[2].dstBinding = 2;
		desc[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc[2].descriptorCount = 1;
		desc[2].pImageInfo = &albedoInfo;

		desc[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc[3].dstSet = textureSet;
		desc[3].dstBinding = 3;
		desc[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc[3].descriptorCount = 1;
		desc[3].pImageInfo = &emissiveInfo;

		constexpr auto numSets = sizeof(desc) / sizeof(desc[0]);
		vkUpdateDescriptorSets(window.device, numSets, desc, 0, nullptr);
	}

	bool recreateSwapchain = false;

	while (!glfwWindowShouldClose(window.window))
	{
		// Let GLFW process events.
		// glfwPollEvents() checks for events, processes them. If there are no
		// events, it will return immediately. Alternatively, glfwWaitEvents()
		// will wait for any event to occur, process it, and only return at
		// that point. The former is useful for applications where you want to
		// render as fast as possible, whereas the latter is useful for
		// input-driven applications, where redrawing is only needed in
		// reaction to user input (or similar).
		glfwPollEvents(); // or: glfwWaitEvents()

		// Recreate swap chain
		if (recreateSwapchain)
		{
			// re-create swapchain and associated resources - see Exercise 3!
			vkDeviceWaitIdle(window.device);
			auto const changes = lut::recreate_swapchain(window);
			if (changes.changedFormat)
			{
				renderPass = create_second_render_pass(window);
			}

			if (changes.changedSize)
			{
				pbrPipe = create_fullscreen_pipeline(window, fullscreen_renderPass.handle, pipeLayout.handle, cfg::kfirstPassVertPath, cfg::kfirstPassFragPath);
			}
			//TODO: (Section 6) re-create depth buffer image
			if (changes.changedSize)
			{
				std::tie(depthBuffer, depthBufferView) = create_depth_buffer(window, allocator);
			}

			framebuffers.clear();
			create_swapchain_framebuffers(window, renderPass.handle, framebuffers, depthBufferView.handle);

			recreateSwapchain = false;
			continue;
		}

		//acquire swapchain image.
		unsigned int imageIndex = 0;
		auto const acquireRes = vkAcquireNextImageKHR(window.device, window.swapchain,
			std::numeric_limits<std::uint64_t>::max(),
			imageAvailable.handle,
			VK_NULL_HANDLE,
			&imageIndex);
		if (VK_SUBOPTIMAL_KHR == acquireRes || VK_ERROR_OUT_OF_DATE_KHR == acquireRes)
		{
			recreateSwapchain = true;
			continue;
		}

		if (VK_SUCCESS != acquireRes)
		{
			throw lut::Error("Unable to acquire enxt swapchain image\n" "vkAcquireNextImageKHR() returned %s", lut::to_string(acquireRes).c_str());
		}

		// wait for command buffer to be available
		assert(std::size_t(imageIndex) < cbfences.size());
		if (auto const res = vkWaitForFences(window.device, 1, &cbfences[imageIndex].handle, VK_TRUE, std::numeric_limits<std::uint64_t>::max()); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to wait for command buffer fence %u\n" "vkWaitForFences() returned %s", imageIndex, lut::to_string(res).c_str());
		}

		if (auto const res = vkResetFences(window.device, 1, &cbfences[imageIndex].handle); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to reset command buffer fence %u\n" "vkResetFences() returned %s", imageIndex, lut::to_string(res).c_str());
		}

		update_scene_uniforms(camera, sceneUniforms, window.swapchainExtent.width, window.swapchainExtent.height);

		// record and submit commands
		assert(std::size_t(imageIndex) < cbuffers.size());
		assert(std::size_t(imageIndex) < framebuffers.size());

		record_commands(
			cbuffers[imageIndex],
			renderPass.handle,
			fullscreen_renderPass.handle,
			framebuffers[imageIndex].handle,
			offscreen_framebuffers[imageIndex].handle,

			pbrPipe.handle,
			window.swapchainExtent,
			materialMesh,

			sceneUBO.buffer,
			pbrBuffers,

			sceneUniforms,

			pipeLayout.handle,
			sceneDescriptors,
			pbrDescriptors
		);

		submit_commands(
			window,
			cbuffers[imageIndex],
			cbfences[imageIndex].handle,
			imageAvailable.handle,
			renderFinished.handle);

		//present rendered images (note: use the present_results() method)
		present_results(window.presentQueue,
			window.swapchain,
			imageIndex,
			renderFinished.handle,
			recreateSwapchain
		);

		camera.updateCameraPosition();
	}

	// Cleanup takes place automatically in the destructors, but we sill need
	// to ensure that all Vulkan commands have finished before that.
	vkDeviceWaitIdle(window.device);

	return 0;
}

catch (std::exception const& eErr)
{
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "Error: %s\n", eErr.what());
	return 1;
}

namespace
{
	void glfw_callback_key_press(GLFWwindow* aWindow, int aKey, int /*aScanCode*/, int aAction, int /*aModifierFlags*/)
	{
		if (GLFW_KEY_ESCAPE == aKey && GLFW_PRESS == aAction)
		{
			glfwSetWindowShouldClose(aWindow, GLFW_TRUE);
		}

		auto* uniforms = reinterpret_cast<glsl::SceneUniform*>(glfwGetWindowUserPointer(aWindow));

		//For user control different light numbers
		if (GLFW_KEY_0 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 0;
		}
		if (GLFW_KEY_1 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 1;
		}
		else if (GLFW_KEY_2 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 2;
		}
		else if (GLFW_KEY_3 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 3;
		}
		else if (GLFW_KEY_4 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 4;
		}

		//For moveable light
		if (GLFW_KEY_SPACE == aKey && GLFW_PRESS == aAction)
		{
			cfg::moveable = !cfg::moveable;
		}

		// forward/backward
		if (glfwGetKey(aWindow, GLFW_KEY_W) == GLFW_PRESS)
			camera.speedZ = 1.0f;
		else if (glfwGetKey(aWindow, GLFW_KEY_S) == GLFW_PRESS)
			camera.speedZ = -1.0f;
		else
		{
			camera.speedZ = 0.f;
		}

		// left/right
		if (glfwGetKey(aWindow, GLFW_KEY_A) == GLFW_PRESS)
			camera.speedX = -1.f;
		else if (glfwGetKey(aWindow, GLFW_KEY_D) == GLFW_PRESS)
			camera.speedX = 1.f;
		else
		{
			camera.speedX = 0.f;
		}

		// up/down
		if (glfwGetKey(aWindow, GLFW_KEY_Q) == GLFW_PRESS)
			camera.speedY = 1.f;
		else if (glfwGetKey(aWindow, GLFW_KEY_E) == GLFW_PRESS)
			camera.speedY = -1.f;
		else
		{
			camera.speedY = 0.f;
		}

		// speed up/down
		if (glfwGetKey(aWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			camera.speedScalar = 2.f;
		else if (glfwGetKey(aWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
			camera.speedScalar = 0.05f;
	}

	void glfw_callback_cursor_pos(GLFWwindow* window, double xPos, double yPos)
	{

		if (cfg::enableMouse == true)
		{
			if (cfg::firstMouse)
			{
				cfg::lastX = xPos;
				cfg::lastY = yPos;
				cfg::firstMouse = false;
			}

			float deltaX, deltaY;
			deltaX = xPos - cfg::lastX;
			deltaY = cfg::lastY - yPos;

			cfg::lastX = xPos;
			cfg::lastY = yPos;

			camera.processMouseMovement(deltaX, deltaY);
		}
	}

	void glfw_callback_mouse_button(GLFWwindow* window, int aButton, int aAction, int)
	{
		if (GLFW_MOUSE_BUTTON_RIGHT == aButton && GLFW_PRESS == aAction)
		{
			cfg::enableMouse = !cfg::enableMouse;
		}
	}

	void update_scene_uniforms(Camera& camera,
		glsl::SceneUniform& aSceneUniforms,
		std::uint32_t aFramebufferWidth,
		std::uint32_t aFramebufferHeight)
	{
		//delta time
		float now = glfwGetTime();
		cfg::delta = now - cfg::last;
		cfg::last = now;

		//initilize SceneUniform members
		float const aspect = aFramebufferWidth / float(aFramebufferHeight);

		aSceneUniforms.projection = glm::perspectiveRH_ZO(   //right hand clip space      z-axis from 0 to 1
			lut::Radians(cfg::kCameraFov).value(),
			aspect,
			cfg::kCameraNear,
			cfg::kCameraFar
		);
		aSceneUniforms.projection[1][1] *= -1.f;
		aSceneUniforms.camera = camera.getViewMatrix();
		aSceneUniforms.camPos = camera.position;
		//aSceneUniforms.camPos = aSceneUniforms.projection * aSceneUniforms.view * aSceneUniforms.model;
		aSceneUniforms.projCam = aSceneUniforms.projection * aSceneUniforms.camera;

		if (cfg::moveable)
		{
			glm::mat4 trans = glm::mat4(1.f);
			glm::mat4 rotate = glm::rotate(trans, glm::radians(20.f * cfg::delta), glm::vec3(0.f, 1.f, 0.f));
			for (int i = 0; i < 4; i++)
				aSceneUniforms.lights[i].position = rotate * aSceneUniforms.lights[i].position;
		}
	}
}


namespace
{
	lut::RenderPass create_second_render_pass(lut::VulkanWindow const& aWindow)
	{
		//Render Pass attachments
		VkAttachmentDescription attachments[1]{};
		attachments[0].format = aWindow.swapchainFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//Supass Definition
		VkAttachmentReference subpassAttachments[1]{};
		subpassAttachments[0].attachment = 0; // this refers to attachments[0]
		subpassAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpasses[1]{};
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = subpassAttachments;
		subpasses[0].pDepthStencilAttachment = nullptr;

		//RenderPass Creation
		//reference the structures above 
		VkRenderPassCreateInfo passInfo{};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.attachmentCount = 1;
		passInfo.pAttachments = attachments; //Render Pass attachments
		passInfo.subpassCount = 1;
		passInfo.pSubpasses = subpasses;     //Supass Definition
		passInfo.dependencyCount = 0;
		passInfo.pDependencies = nullptr;

		VkRenderPass rpass = VK_NULL_HANDLE;
		if (auto const res = vkCreateRenderPass(aWindow.device, &passInfo, nullptr, &rpass); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create render pass\n" "vkCreateRenderPass() returned %s", lut::to_string(res).c_str());
		}

		return lut::RenderPass(aWindow.device, rpass);
	}

	lut::PipelineLayout create_second_pipeline_layout(lut::VulkanContext const& aContext, VkDescriptorSetLayout aSceneLayout, VkDescriptorSetLayout aSecondLayout)
	{
		VkDescriptorSetLayout layouts[] = { aSceneLayout, aSecondLayout };

		//Creating the pipeline layout
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = sizeof(layouts) / sizeof(layouts[0]);
		layoutInfo.pSetLayouts = layouts;
		layoutInfo.pushConstantRangeCount = 0;
		layoutInfo.pPushConstantRanges = nullptr;

		VkPipelineLayout layout = VK_NULL_HANDLE;
		if (auto const res = vkCreatePipelineLayout(aContext.device, &layoutInfo, nullptr, &layout); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create pipeline layout\n" "vkCreatePipelineLayout() returned %s", lut::to_string(res).c_str());
		}
		return lut::PipelineLayout(aContext.device, layout);
	}

	lut::Pipeline create_second_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
	{
		//first step  : load shader modules
		lut::ShaderModule vert = lut::load_shader_module(aWindow, cfg::kVertShaderPath);
		lut::ShaderModule frag = lut::load_shader_module(aWindow, cfg::kFragShaderPath);

		//Shader stages in the pipeline
		//We need two here, vert and frag
		VkPipelineShaderStageCreateInfo  stages[2]{};
		//vertex  [0]
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vert.handle;
		stages[0].pName = "main";

		//fragment [1]
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = frag.handle;
		stages[1].pName = "main";

		VkPipelineVertexInputStateCreateInfo emptyInputState;
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		emptyInputState.vertexAttributeDescriptionCount = 0;
		emptyInputState.pVertexAttributeDescriptions = nullptr;
		emptyInputState.vertexBindingDescriptionCount = 0;
		emptyInputState.pVertexBindingDescriptions = nullptr;
		emptyInputState.flags = 0;

		//for rasterization
		VkPipelineInputAssemblyStateCreateInfo assemblyInfo{};
		assemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		assemblyInfo.primitiveRestartEnable = VK_FALSE;

		//todo list...   Tessellation State   not now

		//viewport state
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = float(aWindow.swapchainExtent.width);
		viewport.height = float(aWindow.swapchainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.extent = VkExtent2D{ aWindow.swapchainExtent.width,
									 aWindow.swapchainExtent.height };
		scissor.offset = VkOffset2D{ 0,0 };

		VkPipelineViewportStateCreateInfo viewportInfo{};
		viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportInfo.viewportCount = 1;
		viewportInfo.pViewports = &viewport;
		viewportInfo.scissorCount = 1;
		viewportInfo.pScissors = &scissor;

		//Rasterization State
		VkPipelineRasterizationStateCreateInfo rasterInfo{};
		rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterInfo.depthClampEnable = VK_FALSE;
		rasterInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;


		//Mulitisampling 
		VkPipelineMultisampleStateCreateInfo sampleInfo{};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		//Color Blend State  
		//mask 
		VkPipelineColorBlendAttachmentState blendStates[1]{};
		blendStates[0].blendEnable = VK_FALSE;
		blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.attachmentCount = sizeof(blendStates)/sizeof(blendStates[0]);
		blendInfo.pAttachments = blendStates;

		//Dynamic States  not now

		//Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;  //vert frag
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &emptyInputState;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &sampleInfo;
		pipelineInfo.pColorBlendState = &blendInfo;
		pipelineInfo.layout = aPipelineLayout;
		pipelineInfo.renderPass = aRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pDynamicState = nullptr;

		VkPipeline pipe = VK_NULL_HANDLE;
		if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
		}

		return lut::Pipeline(aWindow.device, pipe);
	}

	void create_swapchain_framebuffers(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, std::vector<lut::Framebuffer>& aFramebuffers, VkImageView aDepthView)
	{
		assert(aFramebuffers.empty());

		for (int i = 0; i < aWindow.swapViews.size(); i++)
		{
			VkImageView attachments[2] = {
				aWindow.swapViews[i],
				aDepthView
			};

			VkFramebufferCreateInfo fbInfo{};
			fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbInfo.flags = 0;      // normal framebuffer
			fbInfo.renderPass = aRenderPass;
			fbInfo.attachmentCount = 2; //updated
			fbInfo.pAttachments = attachments;
			fbInfo.width = aWindow.swapchainExtent.width;
			fbInfo.height = aWindow.swapchainExtent.height;
			fbInfo.layers = 1;

			VkFramebuffer fb = VK_NULL_HANDLE;
			if (auto const res = vkCreateFramebuffer(aWindow.device, &fbInfo, nullptr, &fb); VK_SUCCESS != res)
			{
				throw lut::Error("Unable to create framebuffer for swap chain image %zu\n" "vkCreateFramebuffer() returned %s", i, lut::to_string(res).c_str());
			}
			aFramebuffers.emplace_back(lut::Framebuffer(aWindow.device, fb));
		}

		assert(aWindow.swapViews.size() == aFramebuffers.size());
	}

	lut::DescriptorSetLayout create_second_render_pass_layout(lut::VulkanWindow const& aWindow)
	{
		VkDescriptorSetLayoutBinding bindings[3]{};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		bindings[2].binding = 2;
		bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[2].descriptorCount = 1;
		bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		//descriptor set layout
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
		layoutInfo.pBindings = bindings;

		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		if (auto const res = vkCreateDescriptorSetLayout(aWindow.device, &layoutInfo, nullptr, &layout); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create descriptor set layout\n" "vkCreateDescriptorSetLayout() returned %s", lut::to_string(res).c_str());
		}

		return lut::DescriptorSetLayout(aWindow.device, layout);
	}

	void record_commands(
		VkCommandBuffer aCmdBuff,
		VkRenderPass aRenderPass,
		VkRenderPass aFullscreenRenderPass,
		VkFramebuffer aFramebuffer,
		VkFramebuffer aFullscreenFramebuffer,

		VkPipeline aPBRPipe,
		VkExtent2D const& aImageExtent,
		ColourMesh& aColourMesh,

		VkBuffer aSceneUBO,
		std::vector<lut::Buffer>& aPBR,
		glsl::SceneUniform const& aSceneUniform,

		VkPipelineLayout aGraphicsLayout,
		VkDescriptorSet aSceneDescriptors,
		std::vector<VkDescriptorSet> aPBRDescriptors
		///------------------------------------
	)
	{
		VkCommandBufferBeginInfo begInfo{};
		begInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // we use one time here
		begInfo.pInheritanceInfo = nullptr;

		if (auto const res = vkBeginCommandBuffer(aCmdBuff, &begInfo); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to begin recording command buffer\n" "vkBeginCommandBuffer() returned %s", lut::to_string(res).c_str());
		}

		//upload scene uniforms
		//we need two barriers here
		lut::buffer_barrier(aCmdBuff, aSceneUBO, VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdUpdateBuffer(aCmdBuff, aSceneUBO, 0, sizeof(glsl::SceneUniform), &aSceneUniform);
		lut::buffer_barrier(aCmdBuff, aSceneUBO, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		for (int i = 0; i < aPBR.size(); i++)
		{
				glsl::PBRuniform pbrUniforms{};
				pbrUniforms.albedo = glm::vec4(newShip.materials[i].albedo, 1.f);
				pbrUniforms.emissive = glm::vec4(newShip.materials[i].emissive, 1.f);
				pbrUniforms.metalness = newShip.materials[i].metalness;
				pbrUniforms.shininess = newShip.materials[i].shininess;

				lut::buffer_barrier(aCmdBuff, aPBR[i].buffer, VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
				vkCmdUpdateBuffer(aCmdBuff, aPBR[i].buffer, 0, sizeof(glsl::PBRuniform), &pbrUniforms);
				lut::buffer_barrier(aCmdBuff, aPBR[i].buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}
		
		//first render pass
		{
			//Render Pass
			VkClearValue clearValues[2]{};
			clearValues[0].color.float32[0] = 0.1f;
			clearValues[0].color.float32[1] = 0.1f;
			clearValues[0].color.float32[2] = 0.1f;
			clearValues[0].color.float32[3] = 1.0f;
			clearValues[1].depthStencil.depth = 1.0f;

			VkRenderPassBeginInfo passInfo{};
			passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passInfo.renderPass = aFullscreenRenderPass;
			passInfo.framebuffer = aFullscreenFramebuffer;
			passInfo.renderArea.offset = VkOffset2D{ 0, 0 };
			passInfo.renderArea.extent = aImageExtent;
			passInfo.clearValueCount = 2;
			passInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(aCmdBuff, &passInfo, VK_SUBPASS_CONTENTS_INLINE);


			//drawing with pipeline
			vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 0, 1, &aSceneDescriptors, 0, nullptr);
			//----------------------------------

			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aPBRPipe);
			// Bind vertex input
			for (int i = 0; i < aColourMesh.positions.size(); i++)
			{
				vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 1, 1, &aPBRDescriptors[newShip.meshes[i].materialIndex], 0, nullptr);

				auto& pos = aColourMesh.positions;
				auto& norm = aColourMesh.normals;
				VkBuffer buffers[2] = { pos[i].buffer, norm[i].buffer };
				VkDeviceSize offsets[2]{};
				vkCmdBindVertexBuffers(aCmdBuff, 0, 2, buffers, offsets);
				vkCmdDraw(aCmdBuff, aColourMesh.vertexCount[i], 1, 0, 0);
			}

			vkCmdEndRenderPass(aCmdBuff);
			//end recording
			if (auto const res = vkEndCommandBuffer(aCmdBuff); VK_SUCCESS != res)
			{
				throw lut::Error("Unable to end recording command buffer\n" "vkEndCommandBuffer() returned %s", lut::to_string(res).c_str());
			}
		}


		//sceond render pass
		{
			//Render Pass
			VkClearValue clearValues[2]{};
			clearValues[0].color.float32[0] = 0.1f;
			clearValues[0].color.float32[1] = 0.1f;
			clearValues[0].color.float32[2] = 0.1f;
			clearValues[0].color.float32[3] = 1.0f;
			clearValues[1].depthStencil.depth = 1.0f;

			VkRenderPassBeginInfo passInfo{};
			passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			passInfo.renderPass = aRenderPass;
			passInfo.framebuffer = aFramebuffer;
			passInfo.renderArea.offset = VkOffset2D{ 0, 0 };
			passInfo.renderArea.extent = aImageExtent;
			passInfo.clearValueCount = 2;
			passInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(aCmdBuff, &passInfo, VK_SUBPASS_CONTENTS_INLINE);


			//drawing with pipeline
			vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 0, 1, &aSceneDescriptors, 0, nullptr);
			//----------------------------------

			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aPBRPipe);
			// Bind vertex input
			for (int i = 0; i < aColourMesh.positions.size(); i++)
			{
				vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 1, 1, &aPBRDescriptors[newShip.meshes[i].materialIndex], 0, nullptr);

				auto& pos = aColourMesh.positions;
				auto& norm = aColourMesh.normals;
				VkBuffer buffers[2] = { pos[i].buffer, norm[i].buffer };
				VkDeviceSize offsets[2]{};
				vkCmdBindVertexBuffers(aCmdBuff, 0, 2, buffers, offsets);
				vkCmdDraw(aCmdBuff, aColourMesh.vertexCount[i], 1, 0, 0);
			}

			vkCmdEndRenderPass(aCmdBuff);
			//end recording
			if (auto const res = vkEndCommandBuffer(aCmdBuff); VK_SUCCESS != res)
			{
				throw lut::Error("Unable to end recording command buffer\n" "vkEndCommandBuffer() returned %s", lut::to_string(res).c_str());
			}
		}
	}

	void submit_commands(lut::VulkanWindow const& aWindow, VkCommandBuffer aCmdBuff, VkFence aFence, VkSemaphore aWaitSemaphore, VkSemaphore aSignalSemaphore)
	{
		VkPipelineStageFlags waitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &aCmdBuff;

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &aWaitSemaphore;
		submitInfo.pWaitDstStageMask = &waitPipelineStages;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &aSignalSemaphore;
		if (auto const res = vkQueueSubmit(aWindow.graphicsQueue, 1, &submitInfo, aFence); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to submit command buffer to queue\n" "vkQueueSubmit() returned %s", lut::to_string(res).c_str());
		}
	}

	void present_results(VkQueue aPresentQueue, VkSwapchainKHR aSwapchain, std::uint32_t aImageIndex, VkSemaphore aRenderFinished, bool& aNeedToRecreateSwapchain)
	{
		//throw lut::Error( "Not yet implemented" ); //TODO: (Section 1/Exercise 3) implement me!
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &aRenderFinished;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &aSwapchain;
		presentInfo.pImageIndices = &aImageIndex;
		presentInfo.pResults = nullptr;

		auto const presentRes = vkQueuePresentKHR(aPresentQueue, &presentInfo);

		if (VK_SUBOPTIMAL_KHR == presentRes || VK_ERROR_OUT_OF_DATE_KHR == presentRes)
		{
			aNeedToRecreateSwapchain = true;
		}
		else if (VK_SUCCESS != presentRes)
		{
			throw lut::Error("Unable present swapchain image %u\n" "vkQueuePresentKHR() returned %s", aImageIndex, lut::to_string(presentRes).c_str());
		}
	}
}


namespace
{
	Camera::Camera(glm::vec3 iPosition, glm::vec3 iTarget, glm::vec3 iWorldUp)
	{
		position = iPosition;
		worldUp = iWorldUp;

	}

	Camera::Camera(glm::vec3 iPosition, float iPitch, float iYaw, glm::vec3 iWorldUp)
	{
		position = iPosition;
		worldUp = iWorldUp;
		pitch = iPitch;
		yaw = iYaw;
		forward.x = glm::cos(pitch) * glm::sin(yaw);
		forward.y = glm::sin(pitch);
		forward.z = glm::cos(pitch) * glm::cos(yaw);
		right = glm::normalize(glm::cross(forward, worldUp));
		up = glm::normalize(glm::cross(right, forward));
	}

	glm::mat4 Camera::getViewMatrix()
	{
		return glm::lookAt(position, position + forward, worldUp);
	}

	void Camera::updateCameraAngle()
	{
		// calculate the new Front vector
		forward.x = glm::cos(pitch) * glm::sin(yaw);
		forward.y = glm::sin(pitch);
		forward.z = glm::cos(pitch) * glm::cos(yaw);
		right = glm::normalize(glm::cross(forward, worldUp));
		up = glm::normalize(glm::cross(right, forward));
	}

	void Camera::updateCameraPosition()
	{
		position += (forward * speedZ * speedScalar + right * speedX * speedScalar + up * speedY * speedScalar) * cfg::delta * 10.f;
	}

	void Camera::processMouseMovement(float xoffset, float yoffset)
	{
		yaw -= xoffset * cfg::delta;
		pitch -= yoffset * cfg::delta;

		// update Front, Right and Up Vectors using the updated Euler angles
		updateCameraAngle();
	}
}

namespace
{
	std::tuple<lut::Image, lut::ImageView> create_depth_buffer(lut::VulkanWindow const& aWindow, lut::Allocator const& aAllocator)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = cfg::kDepthFormat;
		imageInfo.extent.width = aWindow.swapchainExtent.width;
		imageInfo.extent.height = aWindow.swapchainExtent.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		VkImage image = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;

		if (auto const res = vmaCreateImage(aAllocator.allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to allocate depth buffer image.\n" "vmaCreateImage() returned %s", lut::to_string(res).c_str());
		}

		lut::Image depthImage(aAllocator.allocator, image, allocation);

		//create the image view
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = depthImage.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = cfg::kDepthFormat;
		viewInfo.components = VkComponentMapping{};
		viewInfo.subresourceRange = VkImageSubresourceRange{
			VK_IMAGE_ASPECT_DEPTH_BIT,
			0,1,
			0,1
		};

		VkImageView view = VK_NULL_HANDLE;
		if (auto const res = vkCreateImageView(aWindow.device, &viewInfo, nullptr, &view); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create image view\n" "vkCreateImageView() returned %s", lut::to_string(res).c_str());
		}

		return { std::move(depthImage), lut::ImageView(aWindow.device, view) };
	}
}
//EOF vim:syntax=cpp:foldmethod=marker:ts=4:noexpandtab: 
