#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include "renderer.h"
#include "mathUtility.h"
#include "shader.h"

#define STB_RECT_PACK_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION

#include "stb_rect_pack.h"
#include "stb_truetype.h"

using namespace std;

Renderer::Renderer() {
        // initialize all location arrays to -1
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        point_position_loc[i] = point_ambient_loc[i] = point_diffuse_loc[i] = point_specular_loc[i] =
        point_constant_loc[i] = point_linear_loc[i] = point_quadratic_loc[i] = -1;
    }
    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
        spot_position_loc[i] = spot_direction_loc[i] = spot_cutoff_loc[i] = spot_outercutoff_loc[i] =
        spot_ambient_loc[i] = spot_diffuse_loc[i] = spot_specular_loc[i] =
        spot_constant_loc[i] = spot_linear_loc[i] = spot_quadratic_loc[i] = -1;
    }
    num_point_lights_loc = -1;
    num_spot_lights_loc = -1;
}

bool Renderer::truetypeInit(const std::string& fontFile) {

    // Read the font file
    ifstream inputStream(fontFile.c_str(), std::ios::binary);

    if (inputStream.fail()) {
        printf("\nError opening font file.\n");
        return(false);
    }

    inputStream.seekg(0, std::ios::end);
    auto&& fontFileSize = inputStream.tellg();
    inputStream.seekg(0, std::ios::beg);

    uint8_t* fontDataBuf = new uint8_t[fontFileSize];

    inputStream.read((char*)fontDataBuf, fontFileSize);

    if (!fontDataBuf) {
        cerr << "Failed to load buffer with font data\n";
        return false;
    }

    if (!stbtt_InitFont(&font.info, fontDataBuf, 0)) {
        cerr << "stbtt_InitFont() Failed!\n";
        return false;
    }

    inputStream.close();

    constexpr auto TEX_SIZE = 1024; //Font atlast width and height
    uint8_t* fontAtlasTextureData = new uint8_t[TEX_SIZE * TEX_SIZE];
    //auto pixels = std::make_unique<uint8_t[]>(TEX_SIZE * TEX_SIZE);

    stbtt_pack_context pack_context;
    if (!stbtt_PackBegin(&pack_context, fontAtlasTextureData, TEX_SIZE, TEX_SIZE, 0, 1, nullptr)) {
        cerr << "Failed to start font packing\n";
        return false;
    }

    font.size = 128.f;
    stbtt_pack_range range{
            font.size,
            32,
            nullptr,
            96,
            font.packedChars
    };

    if (!stbtt_PackFontRanges(&pack_context, fontDataBuf, 0, &range, 1)) {
        cerr << "Failed to pack font ranges\n";
        return false;
    }

    stbtt_PackEnd(&pack_context);

    for (int i = 0; i < 96; i++)
    {
        float unusedX, unusedY;

        stbtt_GetPackedQuad(
            font.packedChars,               // Array of stbtt_packedchar
            TEX_SIZE,                           // Width of the font atlas texture
            TEX_SIZE,                           // Height of the font atlas texture
            i,                            // Index of the glyph
            &unusedX, &unusedY,         // current position of the glyph in screen pixel coordinates, (not required as we have a different coordinate system)
            &font.alignedQuads[i],              // stbtt_alligned_quad struct. (this struct mainly consists of the texture coordinates)
            0                        // Allign X and Y position to a integer (doesn't matter because we are not using 'unusedX' and 'unusedY')
        );
    }

    //each glyph quad texture needs just one color channel: 0 in background and 1 for the actual character pixels. Use it for alpha blending
    //It creates a texture object in TexObjArray for storing the fontAtlasTexture
    TexObjArray.texture2D_Loader(TEX_SIZE, TEX_SIZE, fontAtlasTextureData);
    GLuint texID_loc = TexObjArray.getNumTextureObjects() - 1; //position of font atlas textureObj in the textureArray;
    printf("The texture object #%d stores fontAtlasTexture\n", texID_loc + 1);
    font.textureId = TexObjArray.getTextureId(texID_loc);

    // configure VAO/VBO for char (glyph) texture aligned quads
    //each vertex will have just the Position attribute containing 4 floats: (vec2 pos, vec2 tex)
    // -----------------------------------
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(2, textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);

    //index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textVBO[1]);
    GLuint quadFaceIndex[] = { 0,1,2,2,3,0 };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadFaceIndex), quadFaceIndex, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}


bool Renderer::setRenderMeshesShaderProg(const std::string& vertShaderPath, const std::string& fragShaderPath) {

    // Shader for models
    Shader shader;
    shader.init();
    program = shader.getProgramIndex();
    shader.compileShader(Shader::VERTEX_SHADER, vertShaderPath);
    shader.compileShader(Shader::FRAGMENT_SHADER, fragShaderPath);

    // set semantics for the shader variables
    glBindFragDataLocation(program, 0, "colorOut");
    glBindAttribLocation(program, Shader::VERTEX_COORD_ATTRIB, "position");
    glBindAttribLocation(program, Shader::NORMAL_ATTRIB, "normal");
    glBindAttribLocation(program, Shader::TEXTURE_COORD_ATTRIB, "texCoord");
    glBindAttribLocation(program, Shader::TANGENT_ATTRIB, "tangent");
	glBindAttribLocation(program, Shader::BITANGENT_ATTRIB, "bitangent");

    glLinkProgram(program);
 
    printf("InfoLog for Model Shaders and Program\n%s\n\n", shader.getAllInfoLogs().c_str());
    if (!shader.isProgramValid())
        printf("GLSL Model Program Not Valid!\n");
        
    glUseProgram(program);
    // fixed bindings
    glUniform1i(glGetUniformLocation(program, "texmap"),         0);
    glUniform1i(glGetUniformLocation(program, "texmap1"),        1);
    glUniform1i(glGetUniformLocation(program, "texmap2"),        2);
    glUniform1i(glGetUniformLocation(program, "texmap3"),        3);
    glUniform1i(glGetUniformLocation(program, "texUnitDiff0"),   4);
    glUniform1i(glGetUniformLocation(program, "texUnitDiff1"),   5);
    glUniform1i(glGetUniformLocation(program, "texUnitSpec"),    6);
    glUniform1i(glGetUniformLocation(program, "texUnitNormal"),  7);
    glUniform1i(glGetUniformLocation(program, "texUnitEmissive"),8);


    // defaults for model flags
    GLint u;
    u = glGetUniformLocation(program, "diffMapCount");    if (u >= 0) glUniform1i(u, 0);
    u = glGetUniformLocation(program, "normalMap");       if (u >= 0) glUniform1i(u, false);
    u = glGetUniformLocation(program, "specularMap");     if (u >= 0) glUniform1i(u, false);
    u = glGetUniformLocation(program, "emissiveMap");     if (u >= 0) glUniform1i(u, false);

    pvm_loc = glGetUniformLocation(program, "m_pvm");
    vm_loc = glGetUniformLocation(program, "m_viewModel");
    normal_loc = glGetUniformLocation(program, "m_normal");
    texMode_loc = glGetUniformLocation(program, "texMode"); // different modes of texturing
    lpos_loc = glGetUniformLocation(program, "l_pos");
    
    tex_loc[0] = glGetUniformLocation(program, "texmap");
    tex_loc[1] = glGetUniformLocation(program, "texmap1");
    tex_loc[2] = glGetUniformLocation(program, "texmap2");
    tex_loc[3] = glGetUniformLocation(program, "texmap3");
    tex_loc[4] = glGetUniformLocation(program, "texUnitDiff0");  // diffuse map 0
    tex_loc[5] = glGetUniformLocation(program, "texUnitDiff1");
    tex_loc[6] = glGetUniformLocation(program, "texUnitSpec");    // optional spec map
    tex_loc[7] = glGetUniformLocation(program, "texUnitNormal");  // optional normal map
    tex_loc[8] = glGetUniformLocation(program, "texUnitEmissive");  // optional emissive map

    normalMap_loc   = glGetUniformLocation(program, "normalMap");    // bool/int
    specularMap_loc = glGetUniformLocation(program, "specularMap");  // bool/int
    emissiveMap_loc   = glGetUniformLocation(program, "emissiveMap");    // bool/int
    diffMapCount_loc   = glGetUniformLocation(program, "diffMapCount");    // int


    // --- query directional light uniforms ---
    dir_direction_loc = glGetUniformLocation(program, "dirLight.direction");
    dir_ambient_loc   = glGetUniformLocation(program, "dirLight.ambient");
    dir_diffuse_loc   = glGetUniformLocation(program, "dirLight.diffuse");
    dir_specular_loc  = glGetUniformLocation(program, "dirLight.specular");

    // --- query point lights array uniforms ---
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        std::ostringstream ss;
        ss << "pointLights[" << i << "].position";
        point_position_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].ambient";
        point_ambient_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].diffuse";
        point_diffuse_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].specular";
        point_specular_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].constant";
        point_constant_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].linear";
        point_linear_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "pointLights[" << i << "].quadratic";
        point_quadratic_loc[i] = glGetUniformLocation(program, ss.str().c_str());
    }

    // --- query spot lights array uniforms ---
    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
        std::ostringstream ss;
        ss << "spotLights[" << i << "].position";
        spot_position_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].direction";
        spot_direction_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].cutOff";
        spot_cutoff_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].outerCutOff";
        spot_outercutoff_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].ambient";
        spot_ambient_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].diffuse";
        spot_diffuse_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].specular";
        spot_specular_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].constant";
        spot_constant_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].linear";
        spot_linear_loc[i] = glGetUniformLocation(program, ss.str().c_str());
        ss.str(""); ss.clear();
        ss << "spotLights[" << i << "].quadratic";
        spot_quadratic_loc[i] = glGetUniformLocation(program, ss.str().c_str());
    }
    
    return(shader.isProgramLinked() && shader.isProgramValid());
}
Renderer::~Renderer() {
    glDeleteProgram(program);
    glDeleteProgram(textProgram);
    for (auto& mesh : myMeshes) glDeleteVertexArrays(1, &(mesh.vao));
    myMeshes.clear(); myMeshes.shrink_to_fit();
 
}

bool Renderer::setRenderTextShaderProg(const std::string& vertShaderPath, const std::string& fragShaderPath) {
   
    Shader shader;    // Shader for rendering True Type Font (ttf) bitmap text
    shader.init();
    textProgram = shader.getProgramIndex();
    shader.compileShader(Shader::VERTEX_SHADER, vertShaderPath);
    shader.compileShader(Shader::FRAGMENT_SHADER, fragShaderPath);

    glLinkProgram(textProgram);
    printf("InfoLog for Text Rendering Shader\n%s\n\n", shader.getAllInfoLogs().c_str());

    if (!shader.isProgramValid()) {
        printf("GLSL Text Program Not Valid!\n");
        exit(1);
    }

    fontPvm_loc = glGetUniformLocation(textProgram, "pvm");
    textColor_loc = glGetUniformLocation(textProgram, "textColor");

    // static font atlas texture binding
    glUseProgram(textProgram);
    glActiveTexture(GL_TEXTURE16);
    glBindTexture(GL_TEXTURE_2D, font.textureId);
    glUniform1i(glGetUniformLocation(textProgram, "fontAtlasTexture"), 16);

    return(shader.isProgramLinked() && shader.isProgramValid());
}


void Renderer::activateRenderMeshesShaderProg() {   //GLSL program to draw the meshes
    glUseProgram(program);
}

void Renderer::setSpotParam(float* coneDir, const float cutOff) {
    GLint loc;
    loc = glGetUniformLocation(program, "coneDir");
    glUniform4fv(loc, 1, coneDir);
    loc = glGetUniformLocation(program, "spotCosCutOff");
    glUniform1f(loc, cutOff);
}

void Renderer::setNightMode(bool nightMode) {
    GLint loc;
    loc = glGetUniformLocation(program, "night_mode");
    if (nightMode)
        glUniform1i(loc, 1);
    else
        glUniform1i(loc, 0);
}

void Renderer::setPLightMode(bool plightMode) {
    GLint loc;
    loc = glGetUniformLocation(program, "plight_mode");
    if (plightMode)
        glUniform1i(loc, 1);
    else
        glUniform1i(loc, 0);
}

void Renderer::setHeadlightsMode(bool headlightsMode) {
    GLint loc;
    loc = glGetUniformLocation(program, "headlights_mode");
    if (headlightsMode)
        glUniform1i(loc, 1);
    else
        glUniform1i(loc, 0);
}

void Renderer::setFogMode(bool fogMode) {
    GLint loc;
    loc = glGetUniformLocation(program, "fog_mode");
    if (fogMode)
        glUniform1i(loc, 1);
    else
        glUniform1i(loc, 0);
}

void Renderer::setLightPos(float* lightPos) {
    glUniform4fv(lpos_loc, 1, lightPos);
}

void Renderer::setIsHud(bool isHud) {
    GLint prevProg = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);

    // make sure this->program is the shader that contains is_Hud
    glUseProgram(program); // program is your shader program handle
    GLint loc = glGetUniformLocation(program, "is_Hud");
    if (loc != -1) glUniform1i(loc, isHud ? 1 : 0);

    glUseProgram(prevProg); // restore previous
}

void Renderer::setIsModel(bool isModel) {
    GLint prevProg = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);

    // make sure this->program is the shader that contains is_Hud
    glUseProgram(program); // program is your shader program handle
    GLint loc = glGetUniformLocation(program, "uIsImportedModel");
    if (loc != -1) glUniform1i(loc, isModel ? 1 : 0);

    glUseProgram(prevProg); // restore previous
}



// helper
static void safeNormalize3(float v[3]) {
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len < 1e-6f) {
        v[0] = 0.0f; v[1] = -1.0f; v[2] = 0.0f;
    } else {
        v[0] /= len; v[1] /= len; v[2] /= len;
    }
}

void Renderer::setDirectionalLight(float* direction, float* ambient, float* diffuse, float* specular) {
    // ensure the correct shader program is active
    glUseProgram(program);

    float dir[3] = { direction[0], direction[1], direction[2] };
    safeNormalize3(dir);

    if (dir_direction_loc >= 0) glUniform3fv(dir_direction_loc, 1, dir);
    if (dir_ambient_loc   >= 0) glUniform3fv(dir_ambient_loc, 1, ambient);
    if (dir_diffuse_loc   >= 0) glUniform3fv(dir_diffuse_loc, 1, diffuse);
    if (dir_specular_loc  >= 0) glUniform3fv(dir_specular_loc, 1, specular);

    if (num_point_lights_loc >= 0) glUniform1i(num_point_lights_loc, MAX_POINT_LIGHTS);
    if (num_spot_lights_loc  >= 0) glUniform1i(num_spot_lights_loc,  MAX_SPOT_LIGHTS);
}

void Renderer::setDirectionalLight(float* direction) {
    float amb[3] = {0.05f,0.05f,0.05f}, diff[3] = {0.4f,0.4f,0.4f}, spec[3] = {0.7f,0.7f,0.7f};
    setDirectionalLight(direction, amb, diff, spec);
}

void Renderer::setPointLight(int idx, float* position, float* ambient, float* diffuse, float* specular,
                             float constant, float linear, float quadratic) {
    if (idx < 0 || idx >= MAX_POINT_LIGHTS) return;

    glUseProgram(program);

    if (point_position_loc[idx] >= 0) glUniform3fv(point_position_loc[idx], 1, position);
    if (point_ambient_loc[idx]  >= 0) glUniform3fv(point_ambient_loc[idx], 1, ambient);
    if (point_diffuse_loc[idx]  >= 0) glUniform3fv(point_diffuse_loc[idx], 1, diffuse);
    if (point_specular_loc[idx] >= 0) glUniform3fv(point_specular_loc[idx], 1, specular);
    if (point_constant_loc[idx] >= 0) glUniform1f(point_constant_loc[idx], constant);
    if (point_linear_loc[idx]   >= 0) glUniform1f(point_linear_loc[idx], linear);
    if (point_quadratic_loc[idx]>= 0) glUniform1f(point_quadratic_loc[idx], quadratic);

    if (num_point_lights_loc >= 0) glUniform1i(num_point_lights_loc, MAX_POINT_LIGHTS);
}

void Renderer::setPointLight(int idx, float* position) {
    float amb[3] = {0.02f,0.02f,0.02f}, diff[3] = {0.6f,0.5f,0.4f}, spec[3] = {0.5f,0.5f,0.5f};
    setPointLight(idx, position, amb, diff, spec, 1.0f, 0.09f, 0.032f);
}

void Renderer::setSpotLight(int idx, float* position, float* direction,
                            float cutOff, float outerCutOff,
                            float* ambient, float* diffuse, float* specular,
                            float constant, float linear, float quadratic) {
    if (idx < 0 || idx >= MAX_SPOT_LIGHTS) return;

    glUseProgram(program);

    float dir[3] = { direction[0], direction[1], direction[2] };
    safeNormalize3(dir);

    if (spot_position_loc[idx] >= 0)     glUniform3fv(spot_position_loc[idx], 1, position);
    if (spot_direction_loc[idx] >= 0)    glUniform3fv(spot_direction_loc[idx], 1, dir);
    if (spot_cutoff_loc[idx] >= 0)       glUniform1f(spot_cutoff_loc[idx], cutOff);
    if (spot_outercutoff_loc[idx] >= 0)  glUniform1f(spot_outercutoff_loc[idx], outerCutOff);
    if (spot_ambient_loc[idx] >= 0)      glUniform3fv(spot_ambient_loc[idx], 1, ambient);
    if (spot_diffuse_loc[idx] >= 0)      glUniform3fv(spot_diffuse_loc[idx], 1, diffuse);
    if (spot_specular_loc[idx] >= 0)     glUniform3fv(spot_specular_loc[idx], 1, specular);
    if (spot_constant_loc[idx] >= 0)     glUniform1f(spot_constant_loc[idx], constant);
    if (spot_linear_loc[idx] >= 0)       glUniform1f(spot_linear_loc[idx], linear);
    if (spot_quadratic_loc[idx] >= 0)    glUniform1f(spot_quadratic_loc[idx], quadratic);

    if (num_spot_lights_loc >= 0) glUniform1i(num_spot_lights_loc, MAX_SPOT_LIGHTS);
}

// in renderer.cpp (include <cstdio> at top)
void Renderer::cacheLightUniformLocations() {
    // ensure the program is active when querying locations
    glUseProgram(program);

    dir_direction_loc = glGetUniformLocation(program, "dirLight.direction");
    dir_ambient_loc   = glGetUniformLocation(program, "dirLight.ambient");
    dir_diffuse_loc   = glGetUniformLocation(program, "dirLight.diffuse");
    dir_specular_loc  = glGetUniformLocation(program, "dirLight.specular");

    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        char name[128];
        snprintf(name, sizeof(name), "pointLights[%d].position", i); point_position_loc[i] = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].ambient", i);  point_ambient_loc[i]  = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].diffuse", i);  point_diffuse_loc[i]  = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].specular", i); point_specular_loc[i] = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].constant", i); point_constant_loc[i] = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].linear", i);   point_linear_loc[i]   = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "pointLights[%d].quadratic", i);point_quadratic_loc[i]= glGetUniformLocation(program, name);
    }

    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i) {
        char name[128];
        snprintf(name, sizeof(name), "spotLights[%d].position", i);        spot_position_loc[i]     = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].direction", i);       spot_direction_loc[i]    = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].cutOff", i);          spot_cutoff_loc[i]       = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].outerCutOff", i);     spot_outercutoff_loc[i]  = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].ambient", i);         spot_ambient_loc[i]      = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].diffuse", i);         spot_diffuse_loc[i]      = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].specular", i);        spot_specular_loc[i]     = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].constant", i);        spot_constant_loc[i]     = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].linear", i);          spot_linear_loc[i]       = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "spotLights[%d].quadratic", i);       spot_quadratic_loc[i]    = glGetUniformLocation(program, name);
    }

    num_point_lights_loc = glGetUniformLocation(program, "numPointLights");
    num_spot_lights_loc  = glGetUniformLocation(program, "numSpotLights");
}


void Renderer::setTexUnit(int tuId, int texObjId) {
    glActiveTexture(GL_TEXTURE0 + tuId);
    glBindTexture(GL_TEXTURE_2D, TexObjArray.getTextureId(texObjId));
    glUniform1i(tex_loc[tuId], tuId);
}

void Renderer::setTexUnit(int tuId, GLuint texId) {
    glActiveTexture(GL_TEXTURE0 + tuId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glUniform1i(tex_loc[tuId], tuId);
}

void Renderer::renderMesh(const dataMesh& data) {
    GLint loc;

    // be aware to activate previously the Model shader program
    glUniformMatrix4fv(vm_loc, 1, GL_FALSE, data.vm);
    glUniformMatrix4fv(pvm_loc, 1, GL_FALSE, data.pvm);
    glUniformMatrix3fv(normal_loc, 1, GL_FALSE, data.normal);

    // send the material
    loc = glGetUniformLocation(program, "mat.ambient");
    glUniform4fv(loc, 1, myMeshes[data.meshID].mat.ambient);
    loc = glGetUniformLocation(program, "mat.diffuse");
    glUniform4fv(loc, 1, myMeshes[data.meshID].mat.diffuse);
    loc = glGetUniformLocation(program, "mat.specular");
    glUniform4fv(loc, 1, myMeshes[data.meshID].mat.specular);
    loc = glGetUniformLocation(program, "mat.shininess");
    glUniform1f(loc, myMeshes[data.meshID].mat.shininess);

    // Render mesh
    glUniform1i(texMode_loc, data.texMode);

    glBindVertexArray(myMeshes[data.meshID].vao);
    glDrawElements(myMeshes[data.meshID].type, myMeshes[data.meshID].numIndexes, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::renderText(const TextCommand& text) {

    glUseProgram(textProgram);   //use GLSL program for text rendering
    glUniformMatrix4fv(fontPvm_loc, 1, GL_FALSE, text.pvm);
    glUniform4fv(textColor_loc, 1, text.color);
    glBindVertexArray(textVAO);

    float localPosition[2] = { text.position[0], text.position[1] };   //screen coordinates

    for (auto ch : text.str) {

        if (ch > 32 && ch < 127) {
            // Retrieve the data that is used to render a glyph of character 'ch'
            stbtt_packedchar packedChar = font.packedChars[ch - 32];
            stbtt_aligned_quad alignedQuad = font.alignedQuads[ch - 32];

            // The units of the fields of the above structs are in pixels,

            float glyphSize[2] = { (float)(packedChar.x1 - packedChar.x0) * text.size,(float)(packedChar.y1 - packedChar.y0) * text.size };
            float glyphBoundingBoxBottomLeft[2] = { localPosition[0] + (packedChar.xoff * text.size), (localPosition[1] - packedChar.yoff2) * text.size };

            // The order of vertices of a quad goes top-right, top-left, bottom-left, bottom-right
            //each vertex will have just the Position attribute containing 4 floats: (vec2 pos, vec2 tex), in total 16 floats
            float glyphVertices[16] = { glyphBoundingBoxBottomLeft[0] + glyphSize[0], glyphBoundingBoxBottomLeft[1] + glyphSize[1], alignedQuad.s1, alignedQuad.t0,
                glyphBoundingBoxBottomLeft[0], glyphBoundingBoxBottomLeft[1] + glyphSize[1], alignedQuad.s0, alignedQuad.t0,
                glyphBoundingBoxBottomLeft[0], glyphBoundingBoxBottomLeft[1], alignedQuad.s0, alignedQuad.t1,
                glyphBoundingBoxBottomLeft[0] + glyphSize[0], glyphBoundingBoxBottomLeft[1], alignedQuad.s1, alignedQuad.t1 };

            // update content of VBO memory
            glBindBuffer(GL_ARRAY_BUFFER, textVBO[0]);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glyphVertices), glyphVertices);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Update the position to render the next glyph specified by packedChar->xadvance.
            localPosition[0] += packedChar.xadvance * text.size;
        }
        // Handle newlines seperately.
        else if (ch == '\n')
        {
            // advance y by fontSize, reset x-coordinate
            localPosition[1] -= 1.0 * font.size * text.size;
            localPosition[0] = text.position[0];
        }
        else if (ch == ' ')
        {
            // advance x by fontSize, keep y-coordinate
            localPosition[0] += 0.2 * font.size * text.size;
        }
    }
}


// GLuint Renderer::getTextureIdFromUnit(int tu) const {
    // return TexObjArray.getTextureId(tu);
// }


