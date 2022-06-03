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

namespace
{

	lut::RenderPass create_fullscreen_render_pass
	(
		lut::VulkanWindow const&,
		VkFormat posFormat,
		VkFormat normFormat,
		VkFormat albedoFormat,
		VkFormat depthFormat
	);

	lut::Pipeline create_fullscreen_pipeline(lut::VulkanWindow const&, VkRenderPass, VkPipelineLayout, char const* aVertPath, char const* aFragPath);

	lut::PipelineLayout create_fullscreen_pipeline_layout(lut::VulkanContext const&, VkDescriptorSetLayout, VkDescriptorSetLayout);

	lut::DescriptorSetLayout create_scene_descriptor_layout(lut::VulkanWindow const&);

	lut::DescriptorSetLayout create_advanced_descriptor_layout(lut::VulkanWindow const&);

	void create_fullscreen_framebuffers(
		lut::Allocator const& aAllocator,
		lut::VulkanWindow const& aWindow,
		VkRenderPass aFullscreenRenderPass,
		std::vector<lut::Framebuffer>& aFramebuffers,
		VkImageView aPositionView,
		VkImageView aNormalView,
		VkImageView aAlbedoView,
		VkImageView aEmissiveView,
		VkImageView aDepthView
	);
}