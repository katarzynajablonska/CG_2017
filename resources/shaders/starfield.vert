#version 150
#extension GL_ARB_explicit_attrib_location : require
// glVertexAttribPointer mapped positions to first
layout(location = 0) in vec3 in_Position;
// glVertexAttribPointer mapped color  to second attribute 
layout(location = 1) in vec3 in_Color;

layout(std140) uniform ubo_data{
    mat4 ubo_view_matrix;
    mat4 ubo_projection_matrix;
};

out vec3 pass_Color;

void main() {
    gl_Position = (ubo_projection_matrix  * ubo_view_matrix) * vec4(in_Position, 1.0);
	pass_Color = in_Color;
}