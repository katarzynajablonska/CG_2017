#version 150

uniform sampler2D tex;

in  vec4 pass_Color;
in  vec2 pass_TexCoords;
out vec4 out_Color;

void main() {
  out_Color = texture(tex, pass_TexCoords) * pass_Color;
    //out_Color = vec4(0.0, 0.0, 1.0, 1.0);
}