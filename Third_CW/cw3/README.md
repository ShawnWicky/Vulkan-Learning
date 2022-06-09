2.1

Between the first render pass and second render pass, I added a subpass dependency

VkSubpassDependency dependencies[1]{};
		dependencies[0].srcSubpass = 0;
		dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

srcSubpass is the current render pass 
dstSubpass used "VK_SUBPASS_EXTERNAL" which is used to synchronize

I choosed to do the deferred shading

2.4

The G-buffer I used:

constexpr VkFormat kPositionFormat = VK_FORMAT_R16G16B16A16_SFLOAT; -->The position info are float numbers that large or equal to one
constexpr VkFormat kNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT; --> The normal info are float numbers that has some number less than 0
constexpr VkFormat kEmissiveFormat = VK_FORMAT_R16G16B16A16_SFLOAT; --> The emissive info combined the shininess which is larger than 1
constexpr VkFormat kAlbedoFormat = VK_FORMAT_R8G8B8A8_SRGB; --> The albedo info combined the metalness which are all between 0 and 1 so used SRGB format
