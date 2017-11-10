#version 150

uniform int flags;
const int SHADE = 1;
const int CEL = 2;

in  vec3 pass_Normal;
in  vec3 pass_Color;
in  vec3 toLight;
in  vec3 toCamera;
out vec4 out_Color;

vec3 Ka = vec3(0.1, 0.1, 0.1);
vec3 Kd = vec3(0.7, 0.7, 0.7);
vec3 Ks = vec3(1.0, 1.0, 1.0);
float shine = 16.0;

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
    if ((flags & SHADE) > 0)
    {
        vec3 l = normalize(toLight);
        vec3 v = normalize(toCamera);
        vec3 n = normalize(pass_Normal);
        out_Color = vec4(pass_Color * (
                                   ambient()
                                   + diffuse(n, l)
                                   + specular(n, l, v)
                                   ), 1.0);
    }
    else
    {
        out_Color = vec4(pass_Color, 1.0);
    }
  //out_Color = vec4(normalize(pass_Normal), 1.0);
}
