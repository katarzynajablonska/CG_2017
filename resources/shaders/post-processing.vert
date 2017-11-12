#version 150
#extension GL_ARB_explicit_attrib_location : require
// vertex attributes of VAO
layout(location = 0) in vec3 in_Position;

const vec2 offset = vec2(0.5, 0.5);
out vec2 texture_coord;

void main(void)
{
    //we assume input vertices form a quad from (-1.0, -1.0) to (1.0, 1.0)
    //we generate apprioprate texture coordinates in (0.0, 0.0) to (1.0, 1.0)
    texture_coord = in_Position.xy * offset + offset;
    
    //the quad has already proper view coordinates, no transformation needed
	gl_Position = vec4(in_Position, 1.0);
}
