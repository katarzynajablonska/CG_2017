#include "application_solar.hpp"
#include "launcher.hpp"

#include "utils.hpp"
#include "shader_loader.hpp"
#include "model_loader.hpp"

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

void StarField::Init()
{
    count = 400;
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

ApplicationSolar::ApplicationSolar(std::string const& resource_path)
 :Application{resource_path}
 ,planet_object{}
{
  initializeGeometry();
  initializeShaderPrograms();
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 35.0f});
    star_field.Init();
}

void ApplicationSolar::render() const {
  // bind shader to upload uniforms
  star_field.render(glm::inverse(m_view_transform), m_view_projection, m_shaders.at("starfield"));
    
  glUseProgram(m_shaders.at("planet").handle);

  /*glm::fmat4 model_matrix = glm::rotate(glm::fmat4{}, float(glfwGetTime()), glm::fvec3{0.0f, 1.0f, 0.0f});
  model_matrix = glm::translate(model_matrix, glm::fvec3{0.0f, 0.0f, -1.0f});
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ModelMatrix"),
                     1, GL_FALSE, glm::value_ptr(model_matrix));

  // extra matrix for normal transformation to keep them orthogonal to surface
  glm::fmat4 normal_matrix = glm::inverseTranspose(glm::inverse(m_view_transform) * model_matrix);
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("NormalMatrix"),
                     1, GL_FALSE, glm::value_ptr(normal_matrix));*/

  // bind the VAO to draw
  glBindVertexArray(planet_object.vertex_AO);
    drawPlanet(0.0f, 0.0f, glm::fmat4{}, 4.0f);
    drawPlanet(3.0f, 1.0f, glm::fmat4{}, 1.0f);
    drawPlanet(7.0f, 0.95f, glm::fmat4{}, 1.5f);
    glm::fmat4 planet_pos = drawPlanet(11.0f, 0.9f, glm::fmat4{}, 0.75f);
    drawPlanet(2.0f, 1.5f, planet_pos, 0.5f);
     drawPlanet(15.0f, 0.85f, glm::fmat4{}, 1.0f);
     drawPlanet(19.0f, 0.8f, glm::fmat4{}, 1.5f);
     drawPlanet(23.0f, 0.7f, glm::fmat4{}, 2.0f);
     drawPlanet(27.0f, 0.65f, glm::fmat4{}, 1.5f);
     drawPlanet(31.0f, 0.6f, glm::fmat4{}, 0.75f);

  // draw bound vertex array using bound shader
 // glDrawElements(planet_object.draw_mode, planet_object.num_elements, model::INDEX.type, NULL);
}

glm::fmat4 ApplicationSolar::drawPlanet(float distance, float rotation, glm::fmat4 position, float scale) const
{
    glm::fmat4 model_matrix = glm::rotate(position, float(glfwGetTime()) * rotation, glm::fvec3{0.0f, 1.0f, 0.0f});
    model_matrix = glm::translate(model_matrix, glm::fvec3{0.0f, 0.0f, -distance});
    model_matrix = glm::scale(model_matrix, glm::fvec3{scale, scale, scale});
    glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ModelMatrix"),
                       1, GL_FALSE, glm::value_ptr(model_matrix));
    
    // extra matrix for normal transformation to keep them orthogonal to surface
    glm::fmat4 normal_matrix = glm::inverseTranspose(glm::inverse(m_view_transform) * model_matrix);
    glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("NormalMatrix"),
                       1, GL_FALSE, glm::value_ptr(normal_matrix));
    
    glDrawElements(planet_object.draw_mode, planet_object.num_elements, model::INDEX.type, NULL);
    
    return model_matrix;

}

void ApplicationSolar::updateView() {
  // vertices are transformed in camera space, so camera transform must be inverted
  glm::fmat4 view_matrix = glm::inverse(m_view_transform);
  // upload matrix to gpu
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ViewMatrix"),
                     1, GL_FALSE, glm::value_ptr(view_matrix));
}

void ApplicationSolar::updateProjection() {
  // upload matrix to gpu
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
  if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, -0.1f});
    updateView();
  }
  else if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 0.1f});
    updateView();
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
  m_shaders.emplace("planet", shader_program{m_resource_path + "shaders/simple.vert",
                                           m_resource_path + "shaders/simple.frag"});
  // request uniform locations for shader program
  m_shaders.at("planet").u_locs["NormalMatrix"] = -1;
  m_shaders.at("planet").u_locs["ModelMatrix"] = -1;
  m_shaders.at("planet").u_locs["ViewMatrix"] = -1;
  m_shaders.at("planet").u_locs["ProjectionMatrix"] = -1;
    
  m_shaders.emplace("starfield", shader_program{m_resource_path + "shaders/starfield.vert",
                                            m_resource_path + "shaders/starfield.frag"});
  // request uniform locations for shader program
  m_shaders.at("starfield").u_locs["ViewMatrix"] = -1;
  m_shaders.at("starfield").u_locs["ProjectionMatrix"] = -1;
}

// load models
void ApplicationSolar::initializeGeometry() {
  model planet_model = model_loader::obj(m_resource_path + "models/sphere.obj", model::NORMAL);

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