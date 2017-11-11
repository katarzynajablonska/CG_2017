#version 150

uniform sampler2D texDiffuse;

uniform int flags;      //control execution of shader
const int SHADE = 1;    //do Phong shading
const int CEL = 2;      //do Cel shading

const int CEL_SHADES = 6;   //number of discrete shades in Cel shading

in  vec3 pass_Normal;
in  vec3 pass_Color;
in  vec3 toLight;
in  vec3 toCamera;
in  vec2 pass_TexCoord;
out vec4 out_Color;

//standard Phong shading parameters
//material is assumed to be fully reflective
vec3 Ka = vec3(0.1, 0.1, 0.1);
vec3 Kd = vec3(0.7, 0.7, 0.7);
vec3 Ks = vec3(1.0, 1.0, 1.0);
float shine = 16.0;

//Cel shading outline parameters
vec3 outline_color = vec3(1.0, 1.0, 1.0);
float unlit_outline_thickness = 0.3;
float lit_outline_thickness = 0.3;

vec3 ambient()
{
    return Ka;
}

vec3 diffuse(vec3 N, vec3 L)
{
    return Kd * clamp(dot(N, L), 0, 1);
}

vec3 specular(vec3 N, vec3 L, vec3 V)
{
    vec3 reflection = reflect(-L, N);
    float cos_angle = max(0.0, dot(V, reflection));
    return Ks * pow(cos_angle, shine);
}

void main() {
    vec3 color = texture(texDiffuse, pass_TexCoord).rgb;
    vec3 l = normalize(toLight);
    vec3 v = normalize(toCamera);
    vec3 n = normalize(pass_Normal);
    if ((flags & SHADE) > 0)
    {
        color *= (
                                   ambient()
                                   + diffuse(n, l)
                                   + specular(n, l, v)
                                   );
    }
    
    if ((flags & CEL) > 0)
    {
        //Cel shading - interior
        color = ceil(color * CEL_SHADES)/CEL_SHADES;
        
        //Cel shading - outline
        if (dot(v, n) < mix(unlit_outline_thickness, lit_outline_thickness, max(0.0, dot(n, l))))
        {
            color = outline_color;
        }
    }
    
    out_Color = vec4(color, 1.0);
  //out_Color = vec4(normalize(pass_Normal), 1.0);
}
