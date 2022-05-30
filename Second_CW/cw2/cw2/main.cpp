#include <volk/volk.h>

#include <tuple>
#include <chrono>
#include <limits>
#include <vector>
#include <stdexcept>

#include <cstdio>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if !defined(GLM_FORCE_RADIANS)
#	define GLM_FORCE_RADIANS
#endif
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include "../labutils/to_string.hpp"
#include "../labutils/vulkan_window.hpp"

#include "../labutils/angle.hpp"
using namespace labutils::literals;

#include "../labutils/error.hpp"
#include "../labutils/vkutil.hpp"
#include "../labutils/vkimage.hpp"
#include "../labutils/vkobject.hpp"
#include "../labutils/vkbuffer.hpp"
#include "../labutils/allocator.hpp" 
namespace lut = labutils;

#include "model.hpp"

namespace
{
	namespace cfg
	{
		// Compiled shader code for the graphics pipeline(s)
		// See sources in cw2/shaders/*. 
#		define SHADERDIR_ "assets/cw2/shaders/"
		constexpr char const* kVertShaderPath = SHADERDIR_ "default.vert.spv";
		constexpr char const* kFragShaderPath = SHADERDIR_ "default.frag.spv";
		
		constexpr char const* kViewDirectionVert = SHADERDIR_ "viewDirection.vert.spv";
		constexpr char const* kViewDirectionFrag = SHADERDIR_ "viewDirection.frag.spv";

		constexpr char const* kLightDirectionVert = SHADERDIR_ "lightDirection.vert.spv";
		constexpr char const* kLightDirectionFrag = SHADERDIR_ "lightDirection.frag.spv";

		constexpr char const* kBlinnPhongVertPath = SHADERDIR_ "BlinnPhong.vert.spv";
		constexpr char const* kBlinnPhongFragPath = SHADERDIR_ "BlinnPhong.frag.spv";

		constexpr char const* kPBRVertPath = SHADERDIR_ "PBR.vert.spv";
		constexpr char const* kPBRFragPath = SHADERDIR_ "PBR.frag.spv";
#		undef SHADERDIR_

#		define ASSETDIR_ "assets/cw2/"
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

		//For Camera
		bool firstMouse = true;
		bool enableMouse = false;
		float lastX;
		float lastY;
		float last = 0.f;
		float delta;
		//For question 2.1
		bool normalDirection = true;
		bool viewDirection = false;
		bool lightDirection = false;

		//For question 2.2
		bool blinnPhong = false;

		//For question 2.3
		bool PBR = false;

		//For question 2.4
		bool moveable = false;
	}

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

		struct MaterialUniform
		{
			// Note: must map to the std140 uniform interface in the fragment
			// shader, so need to be careful about the packing/alignment here!
			glm::vec4 emissive;
			glm::vec4 diffuse;
			glm::vec4 specular;
			float shininess;
		};

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
	ModelData newShip = load_obj_model(cfg::kNewMaterialtestPath);


	// Local functions:

	// GLFW callbacks
	void glfw_callback_key_press(GLFWwindow*, int, int, int, int);

	void glfw_callback_cursor_pos(GLFWwindow* window, double xPos, double yPos);

	void glfw_callback_mouse_button(GLFWwindow* window, int, int, int);

	//float getDeltaTime();
	// Helpers:
	lut::RenderPass create_render_pass(lut::VulkanWindow const&);

	lut::DescriptorSetLayout create_scene_descriptor_layout(lut::VulkanWindow const&);
	lut::DescriptorSetLayout create_blinnPhong_descriptor_layout(lut::VulkanWindow const&);

	lut::PipelineLayout create_pipeline_layout(lut::VulkanContext const&, VkDescriptorSetLayout, VkDescriptorSetLayout);

    //2.1
	lut::Pipeline create_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	lut::Pipeline create_view_direction_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	lut::Pipeline create_light_direction_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	//2.2
	lut::Pipeline create_blinn_phong_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	//2.3
	lut::Pipeline create_PBR_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);

	std::tuple<lut::Image, lut::ImageView> create_depth_buffer(lut::VulkanWindow const&, lut::Allocator const&);

	void create_swapchain_framebuffers(
		lut::VulkanWindow const&,
		VkRenderPass,
		std::vector<lut::Framebuffer>&,
		VkImageView aDepthView
	);

	void update_scene_uniforms(
		Camera& camera,
		glsl::SceneUniform&,
		std::uint32_t aFramebufferWidth,
		std::uint32_t aFramebufferHeight
	);


	void record_commands(
		VkCommandBuffer,
		VkRenderPass,
		VkFramebuffer,
		VkPipeline,
		VkPipeline,
		VkPipeline,
		VkPipeline,
		VkPipeline,
		VkExtent2D const&,
		//--------------------------------------
		ColourMesh&,

		VkBuffer aSceneUBO,
		std::vector<lut::Buffer>&,
		std::vector<lut::Buffer>&,

		glsl::SceneUniform const&,

		VkPipelineLayout,
		VkDescriptorSet aSceneDescriptors,
		std::vector<VkDescriptorSet> aBlinnPhongDescriptors,
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
	lut::RenderPass renderPass = create_render_pass(window);

	//create scene descriptor set layout
	//call create_scene_descriptor_layout
	lut::DescriptorSetLayout sceneLayout = create_scene_descriptor_layout(window);
	lut::DescriptorSetLayout blinnPhongLayout = create_blinnPhong_descriptor_layout(window);
	
	//create pipeline layout
	lut::PipelineLayout pipeLayout = create_pipeline_layout(window, sceneLayout.handle, blinnPhongLayout.handle);
	lut::Pipeline pipe = create_pipeline(window, renderPass.handle, pipeLayout.handle);
	lut::Pipeline viewPipe = create_view_direction_pipeline(window, renderPass.handle, pipeLayout.handle);
	lut::Pipeline lightPipe = create_light_direction_pipeline(window, renderPass.handle, pipeLayout.handle);

	lut::Pipeline blinnPhongPipe = create_blinn_phong_pipeline(window, renderPass.handle, pipeLayout.handle);

	lut::Pipeline pbrPipe = create_PBR_pipeline(window, renderPass.handle, pipeLayout.handle);
	//create depth buffer
	auto [depthBuffer, depthBufferView] = create_depth_buffer(window, allocator);
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

#pragma region secene uniform, light buffer (with thier descriptorSets)
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

#pragma region 2.2 material uniform buffer and descriptors

	std::vector<lut::Buffer> materialBuffers(newShip.materials.size());
	std::vector<VkDescriptorSet> materialDescriptors(newShip.materials.size());
	for (int i = 0; i < newShip.materials.size(); i++)
	{
			materialBuffers[i] = lut::create_buffer(
				allocator,
				sizeof(glsl::MaterialUniform),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VMA_MEMORY_USAGE_GPU_ONLY
			);

			materialDescriptors[i] = lut::alloc_desc_set(window, dpool.handle, blinnPhongLayout.handle);
			{
					VkWriteDescriptorSet desc[1]{};
					VkDescriptorBufferInfo materialInfo{};
					materialInfo.buffer = materialBuffers[i].buffer;
					materialInfo.range = VK_WHOLE_SIZE;

					desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					desc[0].dstSet = materialDescriptors[i];
					desc[0].dstBinding = 0;
					desc[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					desc[0].descriptorCount = 1;
					desc[0].pBufferInfo = &materialInfo;

					constexpr auto numSets = sizeof(desc) / sizeof(desc[0]);
					vkUpdateDescriptorSets(window.device, numSets, desc, 0, nullptr);
			}
	}
#pragma endregion

#pragma region 2.3 material uniform buffers and descriptors
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

		pbrDescriptors[i] = lut::alloc_desc_set(window, dpool.handle, blinnPhongLayout.handle);
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
				renderPass = create_render_pass(window);
			}

			if (changes.changedSize)
			{
				if(cfg::normalDirection)
					lut::Pipeline pipe = create_pipeline(window, renderPass.handle, pipeLayout.handle);
				else if(cfg::normalDirection)
					lut::Pipeline viewPipe = create_view_direction_pipeline(window, renderPass.handle, pipeLayout.handle);
				else if(cfg::lightDirection)
					lut::Pipeline lightPipe = create_light_direction_pipeline(window, renderPass.handle, pipeLayout.handle);
				else if(cfg::blinnPhong)
					lut::Pipeline blinnPhongPipe = create_blinn_phong_pipeline(window, renderPass.handle, pipeLayout.handle);
				else if(cfg::PBR)
					lut::Pipeline pbrPipe = create_PBR_pipeline(window, renderPass.handle, pipeLayout.handle);
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
			framebuffers[imageIndex].handle,
			pipe.handle,
			viewPipe.handle,
			lightPipe.handle,
			blinnPhongPipe.handle,
			pbrPipe.handle,
			window.swapchainExtent,
			materialMesh,

			sceneUBO.buffer,
			materialBuffers,
			pbrBuffers,

			sceneUniforms,

			pipeLayout.handle,
			sceneDescriptors,
			materialDescriptors,
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
		if(GLFW_KEY_1 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 1;
		}
		else if(GLFW_KEY_2 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 2;
		}
		else if(GLFW_KEY_3 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 3;
		}
		else if(GLFW_KEY_4 == aKey && GLFW_PRESS == aAction)
		{
			uniforms->constant = 4;
		}

		//For moveable light
		if (GLFW_KEY_SPACE == aKey && GLFW_PRESS == aAction)
		{
			cfg::moveable = !cfg::moveable;
		}

		//be able to change the state from different questions
		{
			if (GLFW_KEY_N == aKey && GLFW_PRESS == aAction)
			{
				cfg::normalDirection = true;
				cfg::viewDirection = false;
				cfg::lightDirection = false;
				cfg::blinnPhong = false;
				cfg::PBR = false;
			}
			else if (GLFW_KEY_V == aKey && GLFW_PRESS == aAction)
			{
				cfg::viewDirection = true;
				cfg::normalDirection = false;
				cfg::lightDirection = false;
				cfg::blinnPhong = false;
				cfg::PBR = false;
			}
			else if (GLFW_KEY_L == aKey && GLFW_PRESS == aAction)
			{
				cfg::lightDirection = true;
				cfg::normalDirection = false;
				cfg::viewDirection = false;
				cfg::blinnPhong = false;
				cfg::PBR = false;
			}
			else if (GLFW_KEY_B == aKey && GLFW_PRESS == aAction)
			{
				cfg::blinnPhong = true;
				cfg::normalDirection = false;
				cfg::viewDirection = false;
				cfg::lightDirection = false;
				cfg::PBR = false;
			}
			else if (GLFW_KEY_P == aKey && GLFW_PRESS == aAction)
			{
				cfg::PBR = true;
				cfg::blinnPhong = false;
				cfg::normalDirection = false;
				cfg::viewDirection = false;
				cfg::lightDirection = false;
			}
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



	

	void update_scene_uniforms( Camera& camera,
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
	lut::RenderPass create_render_pass(lut::VulkanWindow const& aWindow)
	{
		//Render Pass attachments
		VkAttachmentDescription attachments[2]{};
		attachments[0].format = aWindow.swapchainFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		attachments[1].format = cfg::kDepthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//Supass Definition
		VkAttachmentReference subpassAttachments[1]{};
		subpassAttachments[0].attachment = 0; // this refers to attachments[0]
		subpassAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachment{};
		depthAttachment.attachment = 1; // this refers to attachments[1]
		depthAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpasses[1]{};
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = subpassAttachments;
		subpasses[0].pDepthStencilAttachment = &depthAttachment;

		//RenderPass Creation
		//reference the structures above 
		VkRenderPassCreateInfo passInfo{};
		passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		passInfo.attachmentCount = 2;
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

	lut::PipelineLayout create_pipeline_layout(lut::VulkanContext const& aContext, VkDescriptorSetLayout aSceneLayout, VkDescriptorSetLayout aBlinnPhongLayout)
	{

		VkDescriptorSetLayout layouts[] = { aSceneLayout, aBlinnPhongLayout};

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

#pragma region 2.1
	lut::Pipeline create_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
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

		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo
		VkVertexInputBindingDescription vertexInputs[2]{};
		//position
		vertexInputs[0].binding = 0;
		vertexInputs[0].stride = sizeof(float) * 3;
		vertexInputs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		//normal
		vertexInputs[1].binding = 1;
		vertexInputs[1].stride = sizeof(float) * 3;    //change here 
		vertexInputs[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertexAttributes[2]{};
		//position
		vertexAttributes[0].binding = 0;
		vertexAttributes[0].location = 0;
		vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexAttributes[0].offset = 0;
		//normal
		vertexAttributes[1].binding = 1;
		vertexAttributes[1].location = 1;
		vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;   // change here 
		vertexAttributes[1].offset = 0;
		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo 

		VkPipelineVertexInputStateCreateInfo inputInfo{};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputInfo.vertexBindingDescriptionCount = 2;
		inputInfo.pVertexBindingDescriptions = vertexInputs;
		inputInfo.vertexAttributeDescriptionCount = 2;
		inputInfo.pVertexAttributeDescriptions = vertexAttributes;


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
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;


		//Mulitisampling 
		VkPipelineMultisampleStateCreateInfo sampleInfo{};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		//Depth/Stencil State    not now
		VkPipelineDepthStencilStateCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = VK_TRUE;
		depthInfo.depthWriteEnable = VK_TRUE;
		depthInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthInfo.minDepthBounds = 0.f;
		depthInfo.maxDepthBounds = 1.f;

		//Color Blend State  
			//mask 
		VkPipelineColorBlendAttachmentState blendStates[1]{};
		blendStates[0].blendEnable = VK_FALSE;
		blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.attachmentCount = 1;
		blendInfo.pAttachments = blendStates;

		//Dynamic States  not now

		//Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;  //vert frag
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &inputInfo;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &sampleInfo;
		pipelineInfo.pColorBlendState = &blendInfo;
		pipelineInfo.layout = aPipelineLayout;
		pipelineInfo.renderPass = aRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pDepthStencilState = &depthInfo;
		pipelineInfo.pDynamicState = nullptr;

		VkPipeline pipe = VK_NULL_HANDLE;
		if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
		}

		return lut::Pipeline(aWindow.device, pipe);
	}

	lut::Pipeline create_view_direction_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
	{
		//first step  : load shader modules
		lut::ShaderModule vert = lut::load_shader_module(aWindow, cfg::kViewDirectionVert);
		lut::ShaderModule frag = lut::load_shader_module(aWindow, cfg::kViewDirectionFrag);

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

		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo
		VkVertexInputBindingDescription vertexInputs[2]{};
		//position
		vertexInputs[0].binding = 0;
		vertexInputs[0].stride = sizeof(float) * 3;
		vertexInputs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertexAttributes[2]{};
		//position
		vertexAttributes[0].binding = 0;
		vertexAttributes[0].location = 0;
		vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexAttributes[0].offset = 0;
		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo 

		VkPipelineVertexInputStateCreateInfo inputInfo{};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputInfo.vertexBindingDescriptionCount = 1;
		inputInfo.pVertexBindingDescriptions = vertexInputs;
		inputInfo.vertexAttributeDescriptionCount = 1;
		inputInfo.pVertexAttributeDescriptions = vertexAttributes;


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
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;


		//Mulitisampling 
		VkPipelineMultisampleStateCreateInfo sampleInfo{};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		//Depth/Stencil State    not now
		VkPipelineDepthStencilStateCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = VK_TRUE;
		depthInfo.depthWriteEnable = VK_TRUE;
		depthInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthInfo.minDepthBounds = 0.f;
		depthInfo.maxDepthBounds = 1.f;

		//Color Blend State  
			//mask 
		VkPipelineColorBlendAttachmentState blendStates[1]{};
		blendStates[0].blendEnable = VK_FALSE;
		blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.attachmentCount = 1;
		blendInfo.pAttachments = blendStates;

		//Dynamic States  not now

		//Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;  //vert frag
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &inputInfo;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &sampleInfo;
		pipelineInfo.pColorBlendState = &blendInfo;
		pipelineInfo.layout = aPipelineLayout;
		pipelineInfo.renderPass = aRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pDepthStencilState = &depthInfo;
		pipelineInfo.pDynamicState = nullptr;

		VkPipeline pipe = VK_NULL_HANDLE;
		if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
		}

		return lut::Pipeline(aWindow.device, pipe);
	}
	
	lut::Pipeline create_light_direction_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
	{
		//first step  : load shader modules
		lut::ShaderModule vert = lut::load_shader_module(aWindow, cfg::kLightDirectionVert);
		lut::ShaderModule frag = lut::load_shader_module(aWindow, cfg::kLightDirectionFrag);

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

		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo
		VkVertexInputBindingDescription vertexInputs[1]{};
		//position
		vertexInputs[0].binding = 0;
		vertexInputs[0].stride = sizeof(float) * 3;
		vertexInputs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertexAttributes[1]{};
		//position
		vertexAttributes[0].binding = 0;
		vertexAttributes[0].location = 0;
		vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexAttributes[0].offset = 0;
		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo 

		VkPipelineVertexInputStateCreateInfo inputInfo{};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputInfo.vertexBindingDescriptionCount = 1;
		inputInfo.pVertexBindingDescriptions = vertexInputs;
		inputInfo.vertexAttributeDescriptionCount = 1;
		inputInfo.pVertexAttributeDescriptions = vertexAttributes;


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
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;


		//Mulitisampling 
		VkPipelineMultisampleStateCreateInfo sampleInfo{};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		//Depth/Stencil State    not now
		VkPipelineDepthStencilStateCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = VK_TRUE;
		depthInfo.depthWriteEnable = VK_TRUE;
		depthInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthInfo.minDepthBounds = 0.f;
		depthInfo.maxDepthBounds = 1.f;

		//Color Blend State  
			//mask 
		VkPipelineColorBlendAttachmentState blendStates[1]{};
		blendStates[0].blendEnable = VK_FALSE;
		blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.attachmentCount = 1;
		blendInfo.pAttachments = blendStates;

		//Dynamic States  not now

		//Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;  //vert frag
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &inputInfo;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &sampleInfo;
		pipelineInfo.pColorBlendState = &blendInfo;
		pipelineInfo.layout = aPipelineLayout;
		pipelineInfo.renderPass = aRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pDepthStencilState = &depthInfo;
		pipelineInfo.pDynamicState = nullptr;

		VkPipeline pipe = VK_NULL_HANDLE;
		if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
		}

		return lut::Pipeline(aWindow.device, pipe);
	}
#pragma endregion

#pragma region 2.2
	lut::Pipeline create_blinn_phong_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
	{
		//first step  : load shader modules
		lut::ShaderModule vert = lut::load_shader_module(aWindow, cfg::kBlinnPhongVertPath);
		lut::ShaderModule frag = lut::load_shader_module(aWindow, cfg::kBlinnPhongFragPath);

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

		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo
		VkVertexInputBindingDescription vertexInputs[2]{};
		//position
		vertexInputs[0].binding = 0;
		vertexInputs[0].stride = sizeof(float) * 3;
		vertexInputs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		//normal
		vertexInputs[1].binding = 1;
		vertexInputs[1].stride = sizeof(float) * 3;
		vertexInputs[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		VkVertexInputAttributeDescription vertexAttributes[2]{};
		//position
		vertexAttributes[0].binding = 0;
		vertexAttributes[0].location = 0;
		vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexAttributes[0].offset = 0;
		//normal
		vertexAttributes[1].binding = 1;
		vertexAttributes[1].location = 1;
		vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexAttributes[1].offset = 0;
		//----------------------------------------------------VkPipelineVertexInputStateCreateInfo 

		VkPipelineVertexInputStateCreateInfo inputInfo{};
		inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		inputInfo.vertexBindingDescriptionCount = 2;
		inputInfo.pVertexBindingDescriptions = vertexInputs;
		inputInfo.vertexAttributeDescriptionCount = 2;
		inputInfo.pVertexAttributeDescriptions = vertexAttributes;


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
		rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
		rasterInfo.depthBiasEnable = VK_FALSE;
		rasterInfo.lineWidth = 1.0f;


		//Mulitisampling 
		VkPipelineMultisampleStateCreateInfo sampleInfo{};
		sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		//Depth/Stencil State    not now
		VkPipelineDepthStencilStateCreateInfo depthInfo{};
		depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthInfo.depthTestEnable = VK_TRUE;
		depthInfo.depthWriteEnable = VK_TRUE;
		depthInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthInfo.minDepthBounds = 0.f;
		depthInfo.maxDepthBounds = 1.f;

		//Color Blend State  
		//mask 
		VkPipelineColorBlendAttachmentState blendStates[1]{};
		blendStates[0].blendEnable = VK_FALSE;
		blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blendInfo{};
		blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendInfo.attachmentCount = 1;
		blendInfo.pAttachments = blendStates;

		//Dynamic States  not now

		//Create pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;  //vert frag
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &inputInfo;
		pipelineInfo.pInputAssemblyState = &assemblyInfo;
		pipelineInfo.pViewportState = &viewportInfo;
		pipelineInfo.pRasterizationState = &rasterInfo;
		pipelineInfo.pMultisampleState = &sampleInfo;
		pipelineInfo.pColorBlendState = &blendInfo;
		pipelineInfo.layout = aPipelineLayout;
		pipelineInfo.renderPass = aRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.pTessellationState = nullptr;
		pipelineInfo.pDepthStencilState = &depthInfo;
		pipelineInfo.pDynamicState = nullptr;

		VkPipeline pipe = VK_NULL_HANDLE;
		if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
		{
			throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
		}

		return lut::Pipeline(aWindow.device, pipe);
	}
#pragma endregion 

#pragma region 2.3
	lut::Pipeline create_PBR_pipeline(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, VkPipelineLayout aPipelineLayout)
	{
			//first step  : load shader modules
			lut::ShaderModule vert = lut::load_shader_module(aWindow, cfg::kPBRVertPath);
			lut::ShaderModule frag = lut::load_shader_module(aWindow, cfg::kPBRFragPath);

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

			//----------------------------------------------------VkPipelineVertexInputStateCreateInfo
			VkVertexInputBindingDescription vertexInputs[2]{};
			//position
			vertexInputs[0].binding = 0;
			vertexInputs[0].stride = sizeof(float) * 3;
			vertexInputs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			//normal
			vertexInputs[1].binding = 1;
			vertexInputs[1].stride = sizeof(float) * 3;
			vertexInputs[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			VkVertexInputAttributeDescription vertexAttributes[2]{};
			//position
			vertexAttributes[0].binding = 0;
			vertexAttributes[0].location = 0;
			vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributes[0].offset = 0;
			//normal
			vertexAttributes[1].binding = 1;
			vertexAttributes[1].location = 1;
			vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			vertexAttributes[1].offset = 0;
			//----------------------------------------------------VkPipelineVertexInputStateCreateInfo 

			VkPipelineVertexInputStateCreateInfo inputInfo{};
			inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			inputInfo.vertexBindingDescriptionCount = 2;
			inputInfo.pVertexBindingDescriptions = vertexInputs;
			inputInfo.vertexAttributeDescriptionCount = 2;
			inputInfo.pVertexAttributeDescriptions = vertexAttributes;


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
			rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // like OpenGL
			rasterInfo.depthBiasEnable = VK_FALSE;
			rasterInfo.lineWidth = 1.0f;


			//Mulitisampling 
			VkPipelineMultisampleStateCreateInfo sampleInfo{};
			sampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			sampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			//Depth/Stencil State    not now
			VkPipelineDepthStencilStateCreateInfo depthInfo{};
			depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthInfo.depthTestEnable = VK_TRUE;
			depthInfo.depthWriteEnable = VK_TRUE;
			depthInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			depthInfo.minDepthBounds = 0.f;
			depthInfo.maxDepthBounds = 1.f;

			//Color Blend State  
			//mask 
			VkPipelineColorBlendAttachmentState blendStates[1]{};
			blendStates[0].blendEnable = VK_FALSE;
			blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			VkPipelineColorBlendStateCreateInfo blendInfo{};
			blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blendInfo.attachmentCount = 1;
			blendInfo.pAttachments = blendStates;

			//Dynamic States  not now

			//Create pipeline
			VkGraphicsPipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;  //vert frag
			pipelineInfo.pStages = stages;
			pipelineInfo.pVertexInputState = &inputInfo;
			pipelineInfo.pInputAssemblyState = &assemblyInfo;
			pipelineInfo.pViewportState = &viewportInfo;
			pipelineInfo.pRasterizationState = &rasterInfo;
			pipelineInfo.pMultisampleState = &sampleInfo;
			pipelineInfo.pColorBlendState = &blendInfo;
			pipelineInfo.layout = aPipelineLayout;
			pipelineInfo.renderPass = aRenderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.pTessellationState = nullptr;
			pipelineInfo.pDepthStencilState = &depthInfo;
			pipelineInfo.pDynamicState = nullptr;

			VkPipeline pipe = VK_NULL_HANDLE;
			if (auto const res = vkCreateGraphicsPipelines(aWindow.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipe); VK_SUCCESS != res)
			{
				throw lut::Error("Unable to create graphics pipeline\n" "vkCreateGraphicsPipelines() returned %s", lut::to_string(res).c_str());
			}

			return lut::Pipeline(aWindow.device, pipe);
		
	}
#pragma endregion

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

	lut::DescriptorSetLayout create_scene_descriptor_layout(lut::VulkanWindow const& aWindow)
	{
		VkDescriptorSetLayoutBinding bindings[1]{};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

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

	lut::DescriptorSetLayout create_blinnPhong_descriptor_layout(lut::VulkanWindow const& aWindow)
	{
		VkDescriptorSetLayoutBinding bindings[1]{};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

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
		VkFramebuffer aFramebuffer,
		VkPipeline aGraphicsPipe,
		VkPipeline aViewPipe,
		VkPipeline alightPipe,
		VkPipeline aBlinnPhongPipe,
		VkPipeline aPBRPipe,
		VkExtent2D const& aImageExtent,
		ColourMesh& aColourMesh,

		VkBuffer aSceneUBO,
		std::vector<lut::Buffer>& aMaterial,
		std::vector<lut::Buffer>& aPBR,
		glsl::SceneUniform const& aSceneUniform,

		VkPipelineLayout aGraphicsLayout,
		VkDescriptorSet aSceneDescriptors,
		std::vector<VkDescriptorSet> aMaterialDescriptors,
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

		if (cfg::blinnPhong)
		{
			for (int i = 0; i < aMaterial.size(); i++)
			{
				glsl::MaterialUniform materialuniforms{};
				materialuniforms.diffuse = glm::vec4(newShip.materials[i].diffuse, 1.f);
				materialuniforms.emissive = glm::vec4(newShip.materials[i].emissive, 1.f);
				materialuniforms.specular = glm::vec4(newShip.materials[i].specular, 1.f);
				materialuniforms.shininess = newShip.materials[i].shininess;

				lut::buffer_barrier(aCmdBuff, aMaterial[i].buffer, VK_ACCESS_UNIFORM_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
				vkCmdUpdateBuffer(aCmdBuff, aMaterial[i].buffer, 0, sizeof(glsl::MaterialUniform), &materialuniforms);
				lut::buffer_barrier(aCmdBuff, aMaterial[i].buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
		}
		else if (cfg::PBR)
		{
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
		}
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
		if (cfg::normalDirection)
			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsPipe);
		else if (cfg::viewDirection)
			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aViewPipe);
		else if (cfg::lightDirection)
			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, alightPipe);

		vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 0, 1, &aSceneDescriptors, 0, nullptr);
			//----------------------------------
		if (cfg::blinnPhong)
			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aBlinnPhongPipe);
		else if(cfg::PBR)
			vkCmdBindPipeline(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aPBRPipe);
		// Bind vertex input
		for (int i = 0; i < aColourMesh.positions.size(); i++)
		{
			if (cfg::blinnPhong)
				vkCmdBindDescriptorSets(aCmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, aGraphicsLayout, 1, 1, &aMaterialDescriptors[newShip.meshes[i].materialIndex], 0, nullptr);
			else if(cfg::PBR)
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

	void submit_commands(lut::VulkanWindow const& aWindow, VkCommandBuffer aCmdBuff, VkFence aFence, VkSemaphore aWaitSemaphore, VkSemaphore aSignalSemaphore)
	{
		//throw lut::Error( "Not yet implemented" ); //TODO: (Section 1/Exercise 3) implement me!
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
		position += (forward * speedZ * speedScalar + right * speedX * speedScalar + up * speedY * speedScalar) * cfg::delta * 20.f;
	}

	void Camera::processMouseMovement(float xoffset, float yoffset)
	{
		yaw -= xoffset *  cfg::delta;
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
