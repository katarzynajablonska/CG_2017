#ifndef APPLICATION_SOLAR_HPP
#define APPLICATION_SOLAR_HPP

#include "application.hpp"
#include "model.hpp"
#include "structs.hpp"

// gpu representation of model

struct StarField
{
    std::vector<GLfloat> points;
    std::vector<GLfloat> colors;
    int count;
    GLuint points_vbo;
    GLuint colors_vbo;
    GLuint vba;
    
    void Init();
    void render(const glm::fmat4& view, const glm::fmat4& projection, const shader_program& shader) const;
};

struct Orbit
{
    std::vector<GLfloat> points;
    GLuint points_vbo;
    int count;
    GLuint vba;
    
    void Init();
    void bind(const glm::fmat4& view, const glm::fmat4& projection, const shader_program& shader) const;
    void render(const glm::fmat4& model, const shader_program& shader) const;
};

class ApplicationSolar : public Application {
 public:
  // allocate and initialize objects
  ApplicationSolar(std::string const& resource_path);
  // free allocated objects
  ~ApplicationSolar();

  // update uniform locations and values
  void uploadUniforms();
  // update projection matrix
  void updateProjection();
  // react to key input
  void keyCallback(int key, int scancode, int action, int mods);
  //handle delta mouse movement input
  void mouseCallback(double pos_x, double pos_y);

  // draw all objects
  void render() const;

 protected:
  void initializeShaderPrograms();
  void initializeGeometry();
  void updateView();
  glm::fmat4 drawPlanet(float distance, float rotation, glm::fmat4 position, float scale, glm::fvec3 color, int flags) const;

  // cpu representation of model
  model_object planet_object;
  StarField star_field;
  Orbit orbit;
};

#endif