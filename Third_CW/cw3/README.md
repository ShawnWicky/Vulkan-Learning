2.1

deferred_first_pass: 4 colour attachments, 1 depth attachments
deferred_second_pass: 1 colour attachments, 1 depth attachments

deferred_first_pipe_layout: scene uniform descriptor layout & material uniform descriptor layout
deferred_second_pipe_layout: image descriptor layout

deferred_first_pipe: with vertex input
deferred_second_pipe: without vertex input (the raster face is clockwise)


Between the first render pass and second render pass, I added a subpass dependency to synchronize the first render pass and second render pass. 

The subpass dependency guarantees the order between two subpasses

VkSubpassDependency dependencies[1]{};
		dependencies[0].srcSubpass = 0;
		dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

srcSubpass = 0 means we are on the first index of subpasses

dstSubpass = VK_SUBPASS_EXTERNAL means wait for the subpasses before finishing, and goes to the next subpass

srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT means we need the colour format output from the first render pass to the second render pass

dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT means the colour output should be processed in the pipeline stage of fragment shader in the second render pass

srcAccessMask = VK_ACCESS_MEMORY_READ_BIT is a universal bitmask for the access mask

dstAccessMask = VK_ACCESS_SHADER_READ_BIT means the output will be read by shader

I choosed to do the deferred shading

2.4

The G-buffer I used:

constexpr VkFormat kPositionFormat = VK_FORMAT_R16G16B16A16_SFLOAT; --> The position info are float numbers that large or equal to one
constexpr VkFormat kNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT; --> The normal info are float numbers that has some number less than 0
constexpr VkFormat kEmissiveFormat = VK_FORMAT_R16G16B16A16_SFLOAT; --> The emissive info combined the shininess which is larger than 1
constexpr VkFormat kAlbedoFormat = VK_FORMAT_R8G8B8A8_SRGB; --> The albedo info combined the metalness which are all between 0 and 1 so used SRGB format
