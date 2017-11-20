#version 150
#extension GL_ARB_explicit_attrib_location : require
// vertex attributes of VAO
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Color;
layout(location = 2) in vec2 in_TexCoords;

out vec4 pass_Color;
out vec2 pass_TexCoords;

void main(void)
{    
    //the quad has already proper view coordinates, no transformation needed
    pass_TexCoords = in_TexCoords;
    pass_Color = in_Color;
	gl_Position = vec4(in_Position, 1.0);
}
