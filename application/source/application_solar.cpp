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

//control flags for planet shader execution
//meant to be combined using | operator
enum shader_flags{
    NONE = 0,   //no flag active
    SHADE = 1,  //enable Phong shading
    CEL = 2,     //enable Cel shading
    NORMAL_MAP = 4    //enable normal mapping
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

void StarField::render(const glm::fmat4& view, const glm::fmat4& projection, const shader_program& shader) const
{
    glUseProgram(shader.handle);
    glUniformMatrix4fv(shader.u_locs.at("ViewMatrix"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(shader.u_locs.at("ProjectionMatrix"),
                       1, GL_FALSE, glm::value_ptr(projection));
    
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

void Orbit::bind(const glm::fmat4& view, const glm::fmat4& projection, const shader_program& shader) const
{
    glUseProgram(shader.handle);
    glUniformMatrix4fv(shader.u_locs.at("ViewMatrix"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(shader.u_locs.at("ProjectionMatrix"),
                       1, GL_FALSE, glm::value_ptr(projection));
    
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
    initializeGeometry();
    initializeShaderPrograms();
    initializeTextures();
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 35.0f});
    star_field.Init();
    orbit.Init();
}

// helper function that loads a texture from a file and registers it with OpenGL
static GLuint loadTexture(const std::string& name)
{
    pixel_data data = texture_loader::file(name);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0, data.channels, data.channel_type, data.ptr());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    return tex;
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
}

void ApplicationSolar::render() const {
  
  star_field.render(glm::inverse(m_view_transform), m_view_projection, m_shaders.at("starfield"));
    
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
    
  //bind orbit shader and send common uniforms
  orbit.bind(glm::inverse(m_view_transform), m_view_projection, m_shaders.at("orbit"));
    
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

void ApplicationSolar::updateView() {
  // vertices are transformed in camera space, so camera transform must be inverted
  glm::fmat4 view_matrix = glm::inverse(m_view_transform);
  // upload matrix to gpu
  glUseProgram(m_shaders.at("planet").handle);
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ViewMatrix"),
                     1, GL_FALSE, glm::value_ptr(view_matrix));
}

void ApplicationSolar::updateProjection() {
  // upload matrix to gpu
  glUseProgram(m_shaders.at("planet").handle);
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ProjectionMatrix"),
                     1, GL_FALSE, glm::value_ptr(m_view_projection));
}

// update uniform locations
void ApplicationSolar::uploadUniforms() {
  updateUniformLocations();
  
  // bind new shader
  glUseProgram(m_shaders.at("planet").handle);
  
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
  m_shaders.at("planet").u_locs["ViewMatrix"] = -1;
  m_shaders.at("planet").u_locs["ProjectionMatrix"] = -1;
  m_shaders.at("planet").u_locs["LightPosition"] = -1;
  m_shaders.at("planet").u_locs["Color"] = -1;
  m_shaders.at("planet").u_locs["flags"] = -1;
  m_shaders.at("planet").u_locs["texDiffuse"] = -1;
  m_shaders.at("planet").u_locs["texNormal"] = -1;
    
  // shader for stars
  m_shaders.emplace("starfield", shader_program{m_resource_path + "shaders/starfield.vert",
                                            m_resource_path + "shaders/starfield.frag"});
  // request uniform locations for shader program
  m_shaders.at("starfield").u_locs["ViewMatrix"] = -1;
  m_shaders.at("starfield").u_locs["ProjectionMatrix"] = -1;
    
  // shader for orbits
  m_shaders.emplace("orbit", shader_program{m_resource_path + "shaders/orbit.vert",
        m_resource_path + "shaders/orbit.frag"});
    
  // request uniform locations for shader program
  m_shaders.at("orbit").u_locs["ModelMatrix"] = -1;
  m_shaders.at("orbit").u_locs["ViewMatrix"] = -1;
  m_shaders.at("orbit").u_locs["ProjectionMatrix"] = -1;
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