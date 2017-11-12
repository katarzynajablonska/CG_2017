#version 150
#extension GL_ARB_explicit_attrib_location : require
// glVertexAttribPointer mapped positions to first
layout(location = 0) in vec3 in_Position;

layout(std140) uniform ubo_data{
    mat4 ubo_view_matrix;
    mat4 ubo_projection_matrix;
};

//Matrix Uniforms uploaded with glUniform*
uniform mat4 ModelMatrix;

void main() {
	gl_Position = (ubo_projection_matrix  * ubo_view_matrix * ModelMatrix) * vec4(in_Position, 1.0);
}