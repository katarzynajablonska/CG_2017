#version 150

uniform sampler2D texDiffuse;
uniform sampler2D texNormal;

uniform int flags;          //control execution of shader
const int SHADE = 1;        //do Phong shading
const int CEL = 2;          //do Cel shading
const int NORMAL_MAP = 4;   //do normal mapping

const int CEL_SHADES = 6;   //number of discrete shades in Cel shading

in  vec3 pass_Normal;
in  vec3 pass_Color;
in  vec3 toLight;
in  vec3 toCamera;
in  vec2 pass_TexCoord;
in  mat3 TBN;
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
        if ((flags & NORMAL_MAP) > 0)
        {
            //normal mapping calculations are done in tangent space
            //we convert necessary vectors here
            vec3 ts_n = normalize(texture(texNormal, pass_TexCoord).rgb * 2.0 - 1.0);
            vec3 ts_l = normalize(TBN * l);
            vec3 ts_v = normalize(TBN * v);
            vec3 shading = ambient();
            
            //we check the original surface direction to avoid shading areas
            //that are on the dark side of the planet, but have negative z-component in the normal map
            if (dot(n, l) > 0.0)
            {
                shading += diffuse(ts_n, ts_l);
                shading += specular(ts_n, ts_l, ts_v);
            }
            color *= shading;
        }
        else
        {
            color *= (
                      ambient()
                      + diffuse(n, l)
                      + specular(n, l, v)
                     );
        }
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
    //out_Color = vec4(n, 1.0);
  //out_Color = vec4(normalize(pass_Normal), 1.0);
}
