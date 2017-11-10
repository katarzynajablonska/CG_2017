#version 150
#extension GL_ARB_explicit_attrib_location : require
// vertex attributes of VAO
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;

//Matrix Uniforms as specified with glUniformMatrix4fv
uniform mat4 ModelMatrix;
uniform mat4 ViewMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 NormalMatrix;
uniform vec3 LightPosition;
uniform vec3 Color;

out vec3 pass_Normal;
out vec3 pass_Color;
out vec3 toLight;
out vec3 toCamera;

void main(void)
{
	gl_Position = (ProjectionMatrix  * ViewMatrix * ModelMatrix) * vec4(in_Position, 1.0);
    vec4 worldPosition = (ViewMatrix * ModelMatrix) * vec4(in_Position, 1.0);
    toCamera = normalize(-worldPosition.xyz);
    toLight = normalize((ViewMatrix * vec4(LightPosition, 1.0)).xyz - worldPosition.xyz);
    
    pass_Normal = normalize((NormalMatrix * vec4(in_Normal, 1.0)).xyz);
    pass_Color = Color;
}
