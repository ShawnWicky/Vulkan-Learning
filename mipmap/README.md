usage of "vkCmdBiltImage" :
  1. create the image use vkImage
  2. create the total number of the mip levels of the image
  3. copy buffer to image through VkBufferImageCopy instance
  4. 
  5. transfer the image layout from UNDEFINED to TRANSFER_SRC_OPTIMAL (GENERAL layout is too slow) - through the VkImageMemoryBarrier and "vkCmdPipelineBarrier"
  6. loop through all mip levels
  7. transfer the image layout from DST to SRC through VkImageMemoryBarrier
  8. create VkImageBlit instance
  9. vkCmdBlitImage()
  10. transfer the image layout from SRC tp SHADER_READ_ONLY_OPTIMAL through VkImageMemoryBarrier
  11. change the image size and end loop
  12. change the accessflag from TRANSFER_READ_BIT to TRANSFER_WRITE_BIT through VkImageMemoryBarrier
  13. remember to record and end command buffer
