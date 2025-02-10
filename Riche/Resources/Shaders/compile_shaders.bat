C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o LightingVS.spv -V LightingVS.vert
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o LightingPS.spv -V LightingPS.frag

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o ObjectIdVS.spv -V ObjectIdVS.vert
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o ObjectIdPS.spv -V ObjectIdPS.frag

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o DepthOnlyVS.spv -V DepthOnlyVS.vert

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o BoundingBoxVS.spv -V BoundingBoxVS.vert
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o BoundingBoxPS.spv -V BoundingBoxPS.frag

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o ViewFrustumCullingCS.spv -V ViewFrustumCullingCS.comp
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o HiZOcclusionCullingCS.spv -V  HiZOcclusionCullingCS.comp

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RenderingQuadVS.spv -V RenderingQuadVS.vert
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RenderingQuadPS.spv -V RenderingQuadPS.frag

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe --version

C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RaytracingShadow/Raygen.rgen.spv -V --target-env vulkan1.3 RaytracingShadow/Raygen.rgen
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RaytracingShadow/ClosestHit.rchit.spv -V --target-env vulkan1.3 RaytracingShadow/ClosestHit.rchit
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RaytracingShadow/Miss.rmiss.spv -V --target-env vulkan1.3 RaytracingShadow/Miss.rmiss
C:/VulkanSDK/1.3.290.0/Bin/glslangValidator.exe -o RaytracingShadow/Shadow.rmiss.spv -V --target-env vulkan1.3 RaytracingShadow/Shadow.rmiss



pause