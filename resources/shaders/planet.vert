#version 150
#extension GL_ARB_explicit_attrib_location : require
// vertex attributes of VAO
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec3 in_Tangent;

layout(std140) uniform ubo_data{
    mat4 ubo_view_matrix;
    mat4 ubo_projection_matrix;
};

//Matrix Uniforms as specified with glUniformMatrix4fv
uniform mat4 ModelMatrix;
uniform mat4 NormalMatrix;
uniform vec3 LightPosition;
uniform vec3 Color;

out vec3 pass_Normal;
out vec2 pass_TexCoord;
out vec3 pass_Color;
out mat3 TBN;
out vec3 toLight;
out vec3 toCamera;

void main(void)
{
    gl_Position = (ubo_projection_matrix * ubo_view_matrix * ModelMatrix) * vec4(in_Position, 1.0);
    
    //all computation is done in view space
    vec4 viewSpacePosition = (ubo_view_matrix * ModelMatrix) * vec4(in_Position, 1.0);
    toCamera = normalize(-viewSpacePosition.xyz); //in view space camera position is always 0.0, 0.0, 0.0
    toLight = normalize((ubo_view_matrix * vec4(LightPosition, 1.0)).xyz - viewSpacePosition.xyz);
    
    pass_Normal = normalize((NormalMatrix * vec4(in_Normal, 0.0)).xyz);
    pass_Color = Color;
    pass_TexCoord = in_TexCoord;
    vec3 tangent = normalize((NormalMatrix * vec4(in_Tangent, 0.0)).xyz);
    vec3 bitangent = cross(pass_Normal, tangent);
    TBN = transpose(mat3(tangent, bitangent, pass_Normal));
}
