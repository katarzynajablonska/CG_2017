#include "application_solar.hpp"
#include "launcher.hpp"

#include "utils.hpp"
#include "shader_loader.hpp"
#include "model_loader.hpp"
#include "texture_loader.hpp"

#include <glbinding/gl/gl.h>
// use gl definitions from glbinding 
using namespace gl;

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include <iostream>

#ifdef __APPLE__
//assuming __APPLE__ means Retina screen which has 4x smaller pixels
static const unsigned int VIEWPORT_WIDTH = 640u * 2;
static const unsigned int VIEWPORT_HEIGHT = 480u * 2;
#else 
static const unsigned int VIEWPORT_WIDTH = 640u;
static const unsigned int VIEWPORT_HEIGHT = 480u;
#endif

//control flags for planet shader execution
//meant to be combined using | operator
enum shader_flags{
    NONE = 0,   //no flag active
    SHADE = 1,  //enable Phong shading
    CEL = 2,     //enable Cel shading
    NORMAL_MAP = 4    //enable normal mapping
};

enum postprocessing_effects{
    FX_NONE = 0,
    FX_FLIP_X = 1,
    FX_FLIP_Y = 2,
    FX_GREYSCALE = 4,
    FX_BLUR = 8
};


void StarField::Init()
{
    count = 400;    //number of stars
    //generate random points on a sphere
    for (int i = 0; i<count; i++)
    {
        glm::fvec3 point = glm::sphericalRand(50.0f);
        points.push_back(point.x);
        points.push_back(point.y);
        points.push_back(point.z);
        
        colors.push_back(glm::linearRand(0.0f, 1.0f));
        colors.push_back(glm::linearRand(0.0f, 1.0f));
        colors.push_back(glm::linearRand(0.0f, 1.0f));
    }
    
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(GL_FLOAT), &points[0], GL_STATIC_DRAW);
    glGenBuffers(1, &colors_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors[0], GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &vba);
    glBindVertexArray(vba);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glBindBuffer(GL_ARRAY_BUFFER, colors_vbo);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    //glPointSize(10.0);
}

void StarField::render(const shader_program& shader) const
{
    glUseProgram(shader.handle);
    
    glBindVertexArray(vba);
    glDrawArrays(GL_POINTS, 0, count);
}

void Orbit::Init()
{
    count = 100;    //number of points forming a circle
    
    //generate equidistant points on a circle
    float step = 2 * M_PI / count;
    for (int i = 0; i<count; i++)
    {
        points.push_back(cos(i * step));
        points.push_back(0.0f);
        points.push_back(sin(i * step));
    }
    
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(GL_FLOAT), &points[0], GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &vba);
    glBindVertexArray(vba);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    
    //glPointSize(10.0);
}

void Orbit::bind(const shader_program& shader) const
{
    glUseProgram(shader.handle);
    
    glBindVertexArray(vba);
}

void Orbit::render(const glm::fmat4& model, const shader_program& shader) const
{
    glUniformMatrix4fv(shader.u_locs.at("ModelMatrix"),
                       1, GL_FALSE, glm::value_ptr(model));
    glDrawArrays(GL_LINE_LOOP, 0, count);
}

ApplicationSolar::ApplicationSolar(std::string const& resource_path)
 :Application{resource_path}
 ,planet_object{}
{
    m_cel = 0;
    m_nmap = 0;
    effect = FX_NONE;
    initializeUBO();
    initializeGeometry();
    initializeShaderPrograms();
    initializeTextures();
    initializeFramebuffer();
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 35.0f});
    star_field.Init();
    orbit.Init();
}

void ApplicationSolar::initializeFramebuffer()
{
    //create and bind off-screen framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    
    //create texture that will be the rendering target
    glGenTextures(1, &screen_texture);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, VIEWPORT_WIDTH, VIEWPORT_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    //create depth buffer for off-screen rendering
    GLuint depth_buffer;
    glGenRenderbuffers(1, &depth_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    //attach depth buffer to framebuffer
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
    
    //attach texture to framebuffer
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, screen_texture, 0);
    
    //select rendering target
    GLenum buffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, buffers);
    
    //check if framebuffer initialisation succeeded
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw std::runtime_error("Failed to initialise framebuffer.");
    }
    
    //restore screen framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// helper function that loads a texture from a file and registers it with OpenGL
static GLuint loadTexture(const std::string& name, bool font = false)
{
    pixel_data data = texture_loader::file(name);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    //font textures need special treatment
    if (font)
    {
        //load texture with alpha channel
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.width, data.height, 0, data.channels, data.channel_type, data.ptr());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        
        //ensure the texture doesn't wrap around at the edges - this causes artifacts
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0, data.channels, data.channel_type, data.ptr());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    return tex;
}

void ApplicationSolar::initializeUBO()
{
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_data), &ubo_data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ApplicationSolar::initializeTextures()
{
    // diffuse maps
    m_textures.insert(std::pair<std::string, GLuint>("sun", loadTexture(m_resource_path + "textures/sunmap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("mercury", loadTexture(m_resource_path + "textures/mercurymap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("venus", loadTexture(m_resource_path + "textures/venusmap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("earth", loadTexture(m_resource_path + "textures/earthmap1k.png")));
    m_textures.insert(std::pair<std::string, GLuint>("mars", loadTexture(m_resource_path + "textures/marsmap1k.png")));
    m_textures.insert(std::pair<std::string, GLuint>("jupiter", loadTexture(m_resource_path + "textures/jupitermap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("saturn", loadTexture(m_resource_path + "textures/saturnmap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("uranus", loadTexture(m_resource_path + "textures/uranusmap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("neptune", loadTexture(m_resource_path + "textures/neptunemap.png")));
    m_textures.insert(std::pair<std::string, GLuint>("pluto", loadTexture(m_resource_path + "textures/plutomap1k.png")));
    m_textures.insert(std::pair<std::string, GLuint>("moon", loadTexture(m_resource_path + "textures/moonmap1k.png")));
    m_textures.insert(std::pair<std::string, GLuint>("sky", loadTexture(m_resource_path + "textures/sky.png")));
    
    //normal maps
    m_textures.insert(std::pair<std::string, GLuint>("earth_normal", loadTexture(m_resource_path + "textures/earth_normal.png")));
    m_textures.insert(std::pair<std::string, GLuint>("mars_normal", loadTexture(m_resource_path + "textures/mars_normal.png")));
    m_textures.insert(std::pair<std::string, GLuint>("mercury_normal", loadTexture(m_resource_path + "textures/mercury_normal.png")));
    m_textures.insert(std::pair<std::string, GLuint>("pluto_normal", loadTexture(m_resource_path + "textures/pluto_normal.png")));
    m_textures.insert(std::pair<std::string, GLuint>("venus_normal", loadTexture(m_resource_path + "textures/venus_normal.png")));
    
    m_textures.insert(std::pair<std::string, GLuint>("font_texture", loadTexture(m_resource_path + "textures/a-font.png", true)));
}

void ApplicationSolar::drawText(float x, float y, float size, const std::string& text, glm::fvec4 color) const
{
    // size of a letter in texture coordinates for 16 * 16 letter texture
    float tex_letter_size = 1.0/16;
    
    // storage for our vetrex arrays
    std::vector<GLfloat> positions(text.length() * 3 * 6);
    std::vector<GLfloat> colors(text.length() * 4 * 6, 1.0);
    std::vector<GLfloat> tex_coords(text.length() * 2 * 6);
    
    // drawText operates in (0.0, 0.0) to (800.0, 600.0) space
    // we need to translate it to device-independent space from (-1.0, -1.0) to (1.0, 1.0)
    float x_size = size / 800.0;
    float y_size = size / 600.0;
    float rx = (x / 400.0) - 1.0;
    float ry = (y / 300.0) - 1.0;
    
    // vertex array for our whole text
    GLuint vba;
    
    glGenVertexArrays(1, &vba);
    glBindVertexArray(vba);
    
    // compute vertex data for each letter
    // we will be drawing two triangles for each letter
    for(int i = 0; i < text.length(); i++)
    {
        // compute vetrex positions for a letter
        int p = i * 6 * 3;
        positions[p] = rx + i * x_size; positions[p + 1] = ry;
        positions[p + 3] = rx + (i + 1.0) * x_size; positions[p + 4] = ry;
        positions[p + 6] = rx + i * x_size; positions[p + 7] = ry + y_size;
        positions[p + 9] = rx + i * x_size; positions[p + 10] = ry + y_size;
        positions[p + 12] = rx + (i + 1.0) * x_size; positions[p + 13] = ry;
        positions[p + 15] = rx + (i + 1.0) * x_size; positions[p + 16] = ry + y_size;
        
        // copy the input color to all vertices
        int c = i * 6 * 4;
        colors[c] = colors[c + 4] = colors[c + 8] = colors[c + 12] = colors[c + 16] = colors[c + 20] = color[0];
        colors[c + 1] = colors[c + 5] = colors[c + 9] = colors[c + 13] = colors[c + 17] = colors[c + 21] = color[1];
        colors[c + 2] = colors[c + 6] = colors[c + 10] = colors[c + 14] = colors[c + 18] = colors[c + 22] = color[2];
        colors[c + 3] = colors[c + 7] = colors[c + 11] = colors[c + 15] = colors[c + 19] = colors[c + 23] = color[3];
        
        // compute texture coordinates of the letter
        // the texture is indexed from top left and has letters in ASCII-order
        int tx = text[i] % 16;
        int ty = 15 - text[i] / 16;
        
        // compute texture coordinates for each vertex
        int t = i * 6 * 2;
        
        tex_coords[t] = tx * tex_letter_size; tex_coords[t + 1] = ty * tex_letter_size;
        tex_coords[t + 2] = (tx + 1) * tex_letter_size; tex_coords[t + 3] = ty * tex_letter_size;
        tex_coords[t + 4] = tx * tex_letter_size; tex_coords[t + 5] = (ty + 1) * tex_letter_size;
        tex_coords[t + 6] = tx * tex_letter_size; tex_coords[t + 7] = (ty + 1) * tex_letter_size;
        tex_coords[t + 8] = (tx + 1) * tex_letter_size; tex_coords[t + 9] = ty * tex_letter_size;
        tex_coords[t + 10] = (tx + 1) * tex_letter_size; tex_coords[t + 11] = (ty + 1) * tex_letter_size;
    }
    
    GLuint vbo[3];
    glGenBuffers(3, vbo);
    
    // copy vertex positions to GPU
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), &positions[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    
    // copy vertex colors to GPU
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(float), &colors[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(1);
    
    // copy vertex texture coordinates to GPU
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, tex_coords.size() * sizeof(float), &tex_coords[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(2);
    
    // activate texture and shader
    glUseProgram(m_shaders.at("font").handle);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_shaders.at("font").u_locs.at("tex"), 0);
    glBindTexture(GL_TEXTURE_2D, m_textures.at("font_texture"));
    
    // enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // draw the text
    glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);
    
    // clean up created buffers
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDeleteBuffers(3, vbo);
    glDeleteVertexArrays(1, &vba);

}

void ApplicationSolar::render() const {
    
  //render off-screen
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
  star_field.render(m_shaders.at("starfield"));
    
  // bind shader to upload uniforms
  glUseProgram(m_shaders.at("planet").handle);
    glUniform3fv(m_shaders.at("planet").u_locs.at("LightPosition"), 1, glm::value_ptr(glm::fvec3{0.0f, 0.0f, 0.0f}));

  // bind the planet VAO to draw
  glBindVertexArray(planet_object.vertex_AO);
    
  //draw all planets
  drawPlanet(0.0f, 0.0f, glm::fmat4{}, 3.5f, glm::fvec3{1.0, 0.0, 0.0}, "sun", NONE | m_cel);  //the Sun - emissive source of light, so no Phong shading
    
  drawPlanet(5.0f, 1.0f, glm::fmat4{}, 1.0f, glm::fvec3{0.0, 1.0, 0.0}, "mercury", SHADE | m_cel | m_nmap
             );
  drawPlanet(7.0f, 0.95f, glm::fmat4{}, 1.5f, glm::fvec3{0.0, 0.0, 1.0}, "venus", SHADE | m_cel | m_nmap);
  glm::fmat4 planet_pos = drawPlanet(11.0f, 0.9f, glm::fmat4{}, 0.75f, glm::fvec3{0.9, 0.7, 1.0}, "earth", SHADE | m_cel | m_nmap);
  drawPlanet(2.0f, 1.5f, planet_pos, 0.5f, glm::fvec3{0.4, 0.5 , 0.8}, "moon", SHADE | m_cel); //a moon
  drawPlanet(15.0f, 0.85f, glm::fmat4{}, 1.0f, glm::fvec3{0.5, 0.9, 0.1}, "mars", SHADE | m_cel | m_nmap);
  drawPlanet(19.0f, 0.8f, glm::fmat4{}, 1.5f, glm::fvec3{0.2, 0.3, 1.0}, "jupiter", SHADE | m_cel);
  drawPlanet(23.0f, 0.7f, glm::fmat4{}, 2.0f, glm::fvec3{0.1, 0.6, 0.4}, "saturn", SHADE | m_cel);
  drawPlanet(27.0f, 0.65f, glm::fmat4{}, 1.5f, glm::fvec3{1.0, 0.3, 0.7}, "uranus", SHADE | m_cel);
  drawPlanet(31.0f, 0.6f, glm::fmat4{}, 0.75f, glm::fvec3{0.4, 0.1, 0.9}, "neptune", SHADE | m_cel);
  drawPlanet(36.0f, 0.4f, glm::fmat4{}, 0.6f, glm::fvec3{0.1, 0.5, 0.2}, "pluto", SHADE | m_cel | m_nmap);
  
  // we render skysphere as an inside of a planet with shading disabled
  // the position of the skysphere is always the same as the position of the camera
  glm::fmat4 camera_pos = glm::translate(glm::fmat4{}, glm::vec3(m_view_transform[3]));
  drawPlanet(0.0f, 0.0f, camera_pos, 500.0f, glm::fvec3{1.0, 1.0, 1.0}, "sky", NONE);
    //drawPlanet(0.0f, 0.0f, camera_pos, 500.0f, glm::fvec3{1.0, 1.0, 1.0}, "font_texture", NONE);
    
  //bind orbit shader and send common uniforms
  orbit.bind(m_shaders.at("orbit"));
    
  //draw all orbits
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{5.0f, 5.0f, 5.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{7.0f, 7.0f, 7.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{11.0f, 11.0f, 11.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(planet_pos, glm::fvec3{2.0f, 2.0f, 2.0f}), m_shaders.at("orbit")); // a moon
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{15.0f, 15.0f, 15.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{19.0f, 19.0f, 19.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{23.0f, 23.0f, 23.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{27.0f, 27.0f, 27.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{31.0f, 31.0f, 31.0f}), m_shaders.at("orbit"));
  orbit.render(glm::scale(glm::fmat4{}, glm::fvec3{36.0f, 36.0f, 36.0f}), m_shaders.at("orbit"));
    
    drawText(0, 0, 32, "Test", glm::fvec4{1.0, 0.0, 0.0, 1.0});
    drawText(400, 300, 24, "QWERTZUIOP!/()cjvfnjnvjn22334$%&", glm::vec4{0.0, 1.0, 0.0, 1.0});
    
  //render to screen
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
  glUseProgram(m_shaders.at("rtt").handle);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, screen_texture);
  glUniform1i(m_shaders.at("rtt").u_locs.at("tex"), 0);
  glUniform1i(m_shaders.at("rtt").u_locs.at("effects"), effect);
  glUniform1f(m_shaders.at("rtt").u_locs.at("one_over_screen_width"), 1.0f/VIEWPORT_WIDTH);
  glUniform1f(m_shaders.at("rtt").u_locs.at("one_over_screen_height"), 1.0f/VIEWPORT_HEIGHT);
  glBindVertexArray(quad_vba);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

glm::fmat4 ApplicationSolar::drawPlanet(float distance, float rotation, glm::fmat4 position, float scale, glm::fvec3 color, const std::string& name, int flags) const
{
    glUniform1i(m_shaders.at("planet").u_locs.at("flags"), flags);
    glUniform3fv(m_shaders.at("planet").u_locs.at("Color"), 1, glm::value_ptr(color));
    glm::fmat4 model_matrix = glm::rotate(position, float(glfwGetTime()) * rotation, glm::fvec3{0.0f, 1.0f, 0.0f});
    model_matrix = glm::translate(model_matrix, glm::fvec3{0.0f, 0.0f, -distance});
    model_matrix = glm::scale(model_matrix, glm::fvec3{scale, scale, scale});
    glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ModelMatrix"),
                       1, GL_FALSE, glm::value_ptr(model_matrix));
    
    // extra matrix for normal transformation to keep them orthogonal to surface
    glm::fmat4 normal_matrix = glm::inverseTranspose(glm::inverse(m_view_transform) * model_matrix);
    glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("NormalMatrix"),
                       1, GL_FALSE, glm::value_ptr(normal_matrix));
    
    // texture channel 0 - diffuse map
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_shaders.at("planet").u_locs.at("texDiffuse"), 0);
    glBindTexture(GL_TEXTURE_2D, m_textures.at(name));
    
    
    if ((flags & NORMAL_MAP) > 0)
    {
        glActiveTexture(GL_TEXTURE1);
        glUniform1i(m_shaders.at("planet").u_locs.at("texNormal"), 1);
        glBindTexture(GL_TEXTURE_2D, m_textures.at(name + "_normal"));
    }
    glDrawElements(planet_object.draw_mode, planet_object.num_elements, model::INDEX.type, NULL);
    
    //return planet's transform - if passed later as position, allows for creating moons
    return model_matrix;
}

void ApplicationSolar::updateUBO()
{
    //upload uniform buffer data to GPU
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    GLvoid* ptr = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
    memcpy(ptr, &ubo_data, sizeof(ubo_data));
    glUnmapBuffer(GL_UNIFORM_BUFFER);
}

void ApplicationSolar::updateView() {
  // vertices are transformed in camera space, so camera transform must be inverted
  glm::fmat4 view_matrix = glm::inverse(m_view_transform);

  ubo_data.view_matrix = view_matrix;
  updateUBO();
}

void ApplicationSolar::updateProjection() {
  ubo_data.projection_matrix = m_view_projection;
  updateUBO();
}

// update uniform locations
void ApplicationSolar::uploadUniforms() {
  updateUniformLocations();
  
  // bind new shader
  glUseProgram(m_shaders.at("planet").handle);
  
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  unsigned int block_index = glGetUniformBlockIndex(m_shaders.at("planet").handle, "ubo_data");
  glUniformBlockBinding(m_shaders.at("planet").handle, block_index, 0);
  glBindBufferBase(GL_UNIFORM_BUFFER, block_index, ubo);
    
    // bind new shader
    glUseProgram(m_shaders.at("starfield").handle);
    
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    block_index = glGetUniformBlockIndex(m_shaders.at("starfield").handle, "ubo_data");
    glUniformBlockBinding(m_shaders.at("starfield").handle, block_index, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, block_index, ubo);
    
    // bind new shader
    glUseProgram(m_shaders.at("orbit").handle);
    
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    block_index = glGetUniformBlockIndex(m_shaders.at("orbit").handle, "ubo_data");
    glUniformBlockBinding(m_shaders.at("orbit").handle, block_index, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, block_index, ubo);
    
  updateView();
  updateProjection();
}

// handle key input
void ApplicationSolar::keyCallback(int key, int scancode, int action, int mods) {
  // move camera forward
  if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, -0.1f});
    updateView();
  }
  // move camera backwards
  else if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 0.1f});
    updateView();
  }
  // disable Cel shading
  else if(key == GLFW_KEY_1 && action == GLFW_PRESS)
  {
      m_cel = 0;
  }
  // enable Cel shading
  else if(key == GLFW_KEY_2 && action == GLFW_PRESS)
  {
      m_cel = CEL;
  }
  else if(key == GLFW_KEY_M && action == GLFW_PRESS)
  {
      m_nmap = 0;
  }
    // enable Cel shading
  else if(key == GLFW_KEY_N && action == GLFW_PRESS)
  {
      m_nmap = NORMAL_MAP;
  }
  else if(key == GLFW_KEY_7 && action == GLFW_PRESS)
  {
      effect ^= FX_GREYSCALE;
  }
  else if(key == GLFW_KEY_8 && action == GLFW_PRESS)
  {
      effect ^= FX_FLIP_X;
  }
  else if(key == GLFW_KEY_9 && action == GLFW_PRESS)
  {
      effect ^= FX_FLIP_Y;
  }
  else if(key == GLFW_KEY_0 && action == GLFW_PRESS)
  {
      effect ^= FX_BLUR;
  }
}

//handle delta mouse movement input
void ApplicationSolar::mouseCallback(double pos_x, double pos_y) {
    glm::fmat4 rotation = glm::rotate(glm::fmat4{}, float (pos_y/240.0f), glm::fvec3{1.0f, 0.0f, 0.0f});
    m_view_transform = rotation * m_view_transform;
    updateView();
  // mouse handling
}

// load shader programs
void ApplicationSolar::initializeShaderPrograms() {
    
  // store shader program objects in container
  m_shaders.emplace("planet", shader_program{m_resource_path + "shaders/planet.vert",
                                           m_resource_path + "shaders/planet.frag"});
  // request uniform locations for shader program
  m_shaders.at("planet").u_locs["NormalMatrix"] = -1;
  m_shaders.at("planet").u_locs["ModelMatrix"] = -1;
  m_shaders.at("planet").u_locs["LightPosition"] = -1;
  m_shaders.at("planet").u_locs["Color"] = -1;
  m_shaders.at("planet").u_locs["flags"] = -1;
  m_shaders.at("planet").u_locs["texDiffuse"] = -1;
  m_shaders.at("planet").u_locs["texNormal"] = -1;
    
  // shader for stars
  m_shaders.emplace("starfield", shader_program{m_resource_path + "shaders/starfield.vert",
                                            m_resource_path + "shaders/starfield.frag"});
    
  // shader for orbits
  m_shaders.emplace("orbit", shader_program{m_resource_path + "shaders/orbit.vert",
        m_resource_path + "shaders/orbit.frag"});
    
  // request uniform locations for shader program
  m_shaders.at("orbit").u_locs["ModelMatrix"] = -1;
    
  // shaders for rendering to off-screen buffer
  m_shaders.emplace("rtt", shader_program{m_resource_path + "shaders/post-processing.vert",
      m_resource_path + "shaders/post-processing.frag"});
  m_shaders.at("rtt").u_locs["tex"] = -1;
  m_shaders.at("rtt").u_locs["effects"] = -1;
  m_shaders.at("rtt").u_locs["one_over_screen_width"] = -1;
  m_shaders.at("rtt").u_locs["one_over_screen_height"] = -1;
    
  // shader for font
  m_shaders.emplace("font", shader_program{m_resource_path + "shaders/font.vert",
        m_resource_path + "shaders/font.frag"});
  m_shaders.at("font").u_locs["tex"] = -1;
}

// load models
void ApplicationSolar::initializeGeometry() {
    model planet_model = model_loader::obj(m_resource_path + "models/sphere.obj", model::NORMAL | model::TEXCOORD | model::TANGENT);

  // generate vertex array object
  glGenVertexArrays(1, &planet_object.vertex_AO);
  // bind the array for attaching buffers
  glBindVertexArray(planet_object.vertex_AO);

  // generate generic buffer
  glGenBuffers(1, &planet_object.vertex_BO);
  // bind this as an vertex array buffer containing all attributes
  glBindBuffer(GL_ARRAY_BUFFER, planet_object.vertex_BO);
  // configure currently bound array buffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * planet_model.data.size(), planet_model.data.data(), GL_STATIC_DRAW);

  // activate first attribute on gpu
  glEnableVertexAttribArray(0);
  // first attribute is 3 floats with no offset & stride
  glVertexAttribPointer(0, model::POSITION.components, model::POSITION.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::POSITION]);
  // activate second attribute on gpu
  glEnableVertexAttribArray(1);
  // second attribute is 3 floats with no offset & stride
  glVertexAttribPointer(1, model::NORMAL.components, model::NORMAL.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::NORMAL]);
    
  glEnableVertexAttribArray(2);
  // second attribute is 3 floats with no offset & stride
  glVertexAttribPointer(2, model::TEXCOORD.components, model::TEXCOORD.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::TEXCOORD]);
    
  glEnableVertexAttribArray(3);
  // second attribute is 3 floats with no offset & stride
  glVertexAttribPointer(3, model::TANGENT.components, model::TANGENT.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::TANGENT]);


   // generate generic buffer
  glGenBuffers(1, &planet_object.element_BO);
  // bind this as an vertex array buffer containing all attributes
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planet_object.element_BO);
  // configure currently bound array buffer
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model::INDEX.size * planet_model.indices.size(), planet_model.indices.data(), GL_STATIC_DRAW);

  // store type of primitive to draw
  planet_object.draw_mode = GL_TRIANGLES;
  // transfer number of indices to model object 
  planet_object.num_elements = GLsizei(planet_model.indices.size());
    
    
  // generate data for full-screen quad
  glGenVertexArrays(1, &quad_vba);
  glBindVertexArray(quad_vba);
    
  // 2 triangles forming a quad
  static const GLfloat quadData[] = {
                                      -1.0f, -1.0f, 0.0f,
                                      1.0f, -1.0f, 0.0f,
                                      -1.0f, 1.0f, 0.0f,
                                      -1.0f, 1.0f, 0.0f,
                                      1.0f, -1.0f, 0.0f,
                                      1.0f, 1.0f, 0.0f
                                    };
  glGenBuffers(1, &quad_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
  glEnableVertexAttribArray(0);
}

ApplicationSolar::~ApplicationSolar() {
  glDeleteBuffers(1, &planet_object.vertex_BO);
  glDeleteBuffers(1, &planet_object.element_BO);
  glDeleteVertexArrays(1, &planet_object.vertex_AO);
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationSolar>(argc, argv);
}