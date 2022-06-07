#pragma once

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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if !defined(GLM_FORCE_RADIANS)
#	define GLM_FORCE_RADIANS
#endif
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

namespace deferred
{
	// Compiled shader code for the graphics pipeline(s)
		// See sources in cw3/shaders/*. 
#	define SHADERDIR_ "assets/cw3/shaders/"
	constexpr char const* kPostVertPath = SHADERDIR_ "deferred.vert.spv";
	constexpr char const* kPostFragPath = SHADERDIR_ "deferred.frag.spv";

	constexpr char const* kVertShaderPath = SHADERDIR_ "MRT.vert.spv";
	constexpr char const* kFragShaderPath = SHADERDIR_ "MRT.frag.spv";
#	undef SHADERDIR_


#	define ASSETDIR_ "assets/cw3/"
	constexpr char const* kNewShipPath = ASSETDIR_ "NewShip.obj";
	constexpr char const* kNewMaterialtestPath = ASSETDIR_ "materialtest.obj";
#	undef ASSETDIR_

	constexpr VkFormat kDepthFormat		 = VK_FORMAT_D32_SFLOAT;
	constexpr VkFormat kPositionFormat	 = VK_FORMAT_R8G8B8A8_SFLOAT;
	constexpr VkFormat kNormalFormat	 = VK_FORMAT_R8G8B8A8_SFLOAT;
	constexpr VkFormat kEmissiveFormat	 = VK_FORMAT_B8G8R8A8_SRGB;
	constexpr VkFormat kAlbedoFormat	 = VK_FORMAT_B8G8R8A8_SRGB;
}


namespace
{
	lut::RenderPass create_deferred_first_pass(lut::VulkanWindow const&);
	lut::RenderPass create_deferred_second_pass(lut::VulkanWindow const&);

	lut::PipelineLayout create_deferred_first_layout(lut::VulkanContext const&, VkDescriptorSetLayout, VkDescriptorSetLayout);
	lut::PipelineLayout create_deferred_second_layout(lut::VulkanContext const&, VkDescriptorSetLayout);

	lut::Pipeline create_deferred_first_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);
	lut::Pipeline create_deferred_second_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout);

	void create_deferred_framebuffers(lut::VulkanWindow const& aWindow, VkRenderPass aRenderPass, lut::Framebuffer& aFramebuffers, VkImageView aPosView, VkImageView aNormView, VkImageView aEmissiveView, VkImageView aAlbedoView, VkImageView aDepthView);
}