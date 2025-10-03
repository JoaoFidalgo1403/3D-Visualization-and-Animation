// renderer.h
// Updated: cache and robust set* functions for struct-based lights

#pragma once

#include <vector>
#include <string>
#include <GL/glew.h>
#include "texture.h"
#include "model.h"
#include "stb_truetype.h"

struct dataMesh {
  int meshID = 0;  //mesh ID in the myMeshes array
  float *pvm, *vm, *normal;  //matrices pointers
  int texMode = 0;  //type of shading-> 0:no texturing; 1:modulate diffuse color with texel color; 2:diffuse color is replaced by texel color; 3: multitexturing
};

enum class Align {
    Left,
    Center,
    Right,
    Top = Right,
    Bottom = Left,
};

struct TextCommand {
    std::string str{};
    float position[2];  //screen coordinates
    float size = 1.f;
    float color[4] = {1.f,1.f,1.f, 1.f};
    float *pvm;
    Align align_x = Align::Center, align_y = Align::Center;
};

class Renderer {
public:
  Renderer();
  ~Renderer();

  bool truetypeInit(const std::string &ttf_filepath);  //Initialization of TRUETYPE  for text rendering

  //Setup render meshes GLSL program
  bool setRenderMeshesShaderProg(const std::string &vertShaderPath, const std::string &fragShaderPath);

  void cacheLightUniformLocations();

  // setup text font rasterizer GLSL program
  bool setRenderTextShaderProg(const std::string &vertShaderPath, const std::string &fragShaderPath);

  void activateRenderMeshesShaderProg();

  void renderMesh(const dataMesh &data);

  void renderText(const TextCommand &text);

  void setLightPos(float *lightPos);

  void setSpotParam(float *coneDir, float cutOff);

  void setNightMode(bool nightMode);

  void setPLightMode(bool plightMode);

  void setHeadlightsMode(bool headlightsMode);

  void setFogMode(bool fogMode);

  static const int MAX_POINT_LIGHTS = 7;
  static const int MAX_SPOT_LIGHTS = 4;

  void setDirectionalLight(float* direction, float* ambient, float* diffuse, float* specular);

  void setPointLight(int idx, float* position, float* ambient, float* diffuse, float* specular,
                    float constant, float linear, float quadratic);
  
  void setPointLight(int idx, float* position); // uses default material & attenuation

  void setSpotLight(int idx, float* position, float* direction, float cutOff, float outerCutOff,
                    float* ambient, float* diffuse, float* specular, float constant, float linear,
                     float quadratic);

  void setDirectionalLight(float* direction); // uses default ambient/diffuse/specular

  void setTexUnit(int tuId, int texObjId);



  //Vector with meshes
  std::vector<struct MyMesh> myMeshes;

  /// Object of class Texture that manage an array of Texture Objects
  Texture TexObjArray;

private:

  //Render meshes GLSL program
  GLuint program;

  // Text font rasterizer GLSL program
  GLuint textProgram;

  GLint pvm_loc, vm_loc, normal_loc, lpos_loc, texMode_loc;
  GLint tex_loc[MAX_TEXTURES];
  
  // Directional light uniform locations
  GLint dir_direction_loc = -1, dir_ambient_loc = -1, dir_diffuse_loc = -1, dir_specular_loc = -1;

  // Point lights uniform locations (arrays)
  GLint point_position_loc[MAX_POINT_LIGHTS];
  GLint point_ambient_loc[MAX_POINT_LIGHTS];
  GLint point_diffuse_loc[MAX_POINT_LIGHTS];
  GLint point_specular_loc[MAX_POINT_LIGHTS];
  GLint point_constant_loc[MAX_POINT_LIGHTS];
  GLint point_linear_loc[MAX_POINT_LIGHTS];
  GLint point_quadratic_loc[MAX_POINT_LIGHTS];

  // Spot lights uniform locations (arrays)
  GLint spot_position_loc[MAX_SPOT_LIGHTS];
  GLint spot_direction_loc[MAX_SPOT_LIGHTS];
  GLint spot_cutoff_loc[MAX_SPOT_LIGHTS];
  GLint spot_outercutoff_loc[MAX_SPOT_LIGHTS];
  GLint spot_ambient_loc[MAX_SPOT_LIGHTS];
  GLint spot_diffuse_loc[MAX_SPOT_LIGHTS];
  GLint spot_specular_loc[MAX_SPOT_LIGHTS];
  GLint spot_constant_loc[MAX_SPOT_LIGHTS];
  GLint spot_linear_loc[MAX_SPOT_LIGHTS];
  GLint spot_quadratic_loc[MAX_SPOT_LIGHTS];

  // global count uniform locations (optional)
  GLint num_point_lights_loc = -1;
  GLint num_spot_lights_loc  = -1;

  //render font GLSL program variable locations and VAO
  GLint fontPvm_loc, textColor_loc;
  GLuint textVAO, textVBO[2];

    struct Font {
        float size;
        GLuint textureId;    //font atlas texture object ID stored in TexObjArray
        stbtt_fontinfo info;
        stbtt_packedchar packedChars[96];
        stbtt_aligned_quad alignedQuads[96];
    } font{};
};