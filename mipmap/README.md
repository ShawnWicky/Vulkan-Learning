usage of "vkCmdBiltImage" :
  1. create the image use vkImage
  2. create the total number of the mip levels of the image
  3. transfer the image layout from UNDEFINED to TRANSFER_SRC_OPTIMAL (GENERAL layout is too slow) - through the VkImageMemoryBarrier and "vkCmdPipelineBarrier"
  4. loop through all mip levels
  5. transfer the image layout from DST to SRC through VkImageMemoryBarrier
  6. create VkImageBlit instance
  7. vkCmdBlitImage()
  8. transfer the image layout from SRC tp SHADER_READ_ONLY_OPTIMAL through VkImageMemoryBarrier
  9. change the image size and end loop
  10. change the accessflag from TRANSFER_READ_BIT to TRANSFER_WRITE_BIT through VkImageMemoryBarrier
  11. remember to record and end command buffer
