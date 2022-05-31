2.1-2.5 are finished

Camera class is based on LearnOpenGL website

2.1
Shading space: default world space
Descriptor Set layout: scene layout && material layout (material layout is for material uniform struct used in PBR and blind phong)
Descriptor bindings: scene uniform descriptor has one binding the uScene uniform in shaders;

Scene uniform in the three shaders used 2.1 are modified to match the multiple lights suitation

Three pipelines are created for this question
ViewDirectionPipe --- press "V" to enable viewDirection
LightDirectionPipe --- press "L" to enable LightDirection
NormalDirectionPipe --- press "N" to enable NormalDirection 

2.2
Press "B" to enable Blinn-Phong

using the advancedDescriptorLayout which is the same as PBR

uniform buffer & DescriptorSets: I created a vector of buffers and descriptors to store the material uniforms for each meshes in the ModelData

2.3
Press "P" to enable PBR

2.4 
Light path: the light is orbit around the Y axis

2.5
There are 4 lights in total.
The lights are updated in the update_scene_uniforms() function

2.6

Not finished