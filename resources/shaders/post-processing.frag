#version 150

uniform sampler2D tex;
uniform int effects;    //flags for post-processing effects
uniform float one_over_screen_width;
uniform float one_over_screen_height;

const int FX_FLIP_X = 1;
const int FX_FLIP_Y = 2;
const int FX_GREYSCALE = 4;
const int FX_BLUR = 8;

out vec4 out_Color;

in vec2 texture_coord;

float luminance(vec3 color)
{
    //convert RGB-color to luminance
    //algorithm from chapter 10 of "Graphics Shaders"
    const vec3 W = vec3(0.2125, 0.7154, 0.0721);
    return dot(color, W);
}

//sample texture applying flipping effects if necessary
vec3 sample_texture(vec2 coords)
{
    coords.x = ((effects & FX_FLIP_X) > 0) ? (1.0 - coords.x) : coords.x;
    coords.y = ((effects & FX_FLIP_Y) > 0) ? (1.0 - coords.y) : coords.y;
    return texture(tex, coords).rgb;
}

//sample texture applying Gaussian blur
vec3 sample_blurred(vec2 coords)
{
    vec3 result = vec3(0.0, 0.0, 0.0);
    
    //3x3 Gaussian kernel
    mat3 gaussian = mat3(
                            1.0, 2.0, 1.0,
                            2.0, 4.0, 2.0,
                            1.0, 2.0, 1.0
                         );
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            vec2 tc = vec2(coords.x + (i - 1) * one_over_screen_width, coords.y + (j - 1) * one_over_screen_height);
            result += gaussian[i][j] * sample_texture(tc);
        }
    }
    result /= 16.0;
    return result;
}

void main() {

    vec3 color;
    
    if ((effects & FX_BLUR) > 0)
    {
        color = sample_blurred(texture_coord);
    }
    else
    {
        color = sample_texture(texture_coord);
    }
    
    if ((effects & FX_GREYSCALE) > 0)
    {
        float luma = luminance(color);
        out_Color = vec4(luma, luma, luma, 1.0);
    }
    else
    {
        out_Color = vec4(color, 1.0);
    }
}
