#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include "renderer.h"
#include "mathUtility.h"
#include "shader.h"
#include <vector>
#include <string>

// Assimp
#include "Dependencies/assimp/include/assimp/Importer.hpp"
#include "Dependencies/assimp/include/assimp/scene.h"
#include "Dependencies/assimp/include/assimp/postprocess.h"

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

    glLinkProgram(program);

    printf("InfoLog for Model Shaders and Program\n%s\n\n", shader.getAllInfoLogs().c_str());
    if (!shader.isProgramValid())
        printf("GLSL Model Program Not Valid!\n");

    pvm_loc = glGetUniformLocation(program, "m_pvm");
    vm_loc = glGetUniformLocation(program, "m_viewModel");
    normal_loc = glGetUniformLocation(program, "m_normal");
    texMode_loc = glGetUniformLocation(program, "texMode"); // different modes of texturing
    lpos_loc = glGetUniformLocation(program, "l_pos");
    tex_loc[0] = glGetUniformLocation(program, "texmap");
    tex_loc[1] = glGetUniformLocation(program, "texmap1");
    tex_loc[2] = glGetUniformLocation(program, "texmap2");
    tex_loc[3] = glGetUniformLocation(program, "texmap3");


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



static void fillMaterialFromAi(const aiMaterial* aimat, Material &outMat) {
    aiColor4D c;
    if (AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_AMBIENT, &c)) {
        outMat.ambient[0]=c.r; outMat.ambient[1]=c.g; outMat.ambient[2]=c.b; outMat.ambient[3]=c.a;
    } else { outMat.ambient[0]=0.2f; outMat.ambient[1]=0.2f; outMat.ambient[2]=0.2f; outMat.ambient[3]=1.0f; }
    if (AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_DIFFUSE, &c)) {
        outMat.diffuse[0]=c.r; outMat.diffuse[1]=c.g; outMat.diffuse[2]=c.b; outMat.diffuse[3]=c.a;
    } else { outMat.diffuse[0]=0.8f; outMat.diffuse[1]=0.8f; outMat.diffuse[2]=0.8f; outMat.diffuse[3]=1.0f; }
    if (AI_SUCCESS == aiGetMaterialColor(aimat, AI_MATKEY_COLOR_SPECULAR, &c)) {
        outMat.specular[0]=c.r; outMat.specular[1]=c.g; outMat.specular[2]=c.b; outMat.specular[3]=c.a;
    } else { outMat.specular[0]=0.2f; outMat.specular[1]=0.2f; outMat.specular[2]=0.2f; outMat.specular[3]=1.0f; }
    float shin = 32.0f; aiGetMaterialFloat(aimat, AI_MATKEY_SHININESS, &shin);
    outMat.shininess = shin;
    outMat.texCount = 0;
}

int Renderer::loadModelWithAssimp(const std::string &filepath, int &outMeshCount) {
    outMeshCount = 0;
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath.c_str(),
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_OptimizeMeshes |
        aiProcess_GenUVCoords |
        aiProcess_TransformUVCoords |
        aiProcess_CalcTangentSpace);

    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Assimp failed to load: " << filepath << "\n";
        return -1;
    }

    int firstIndex = static_cast<int>(myMeshes.size());

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!mesh->HasPositions()) continue;

        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> texcoords;
        std::vector<float> tangents;
        std::vector<unsigned int> indices;

        vertices.reserve(mesh->mNumVertices * 4);
        normals.reserve(mesh->mNumVertices * 4);
        texcoords.reserve(mesh->mNumVertices * 4);
        tangents.reserve(mesh->mNumVertices * 4);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            const aiVector3D &v = mesh->mVertices[i];
            vertices.push_back(v.x); vertices.push_back(v.y); vertices.push_back(v.z); vertices.push_back(1.0f);

            if (mesh->HasNormals()) {
                const aiVector3D &n = mesh->mNormals[i];
                normals.push_back(n.x); normals.push_back(n.y); normals.push_back(n.z); normals.push_back(0.0f);
            } else { normals.insert(normals.end(), {0.f,1.f,0.f,0.f}); }

            if (mesh->HasTextureCoords(0)) {
                const aiVector3D &t = mesh->mTextureCoords[0][i];
                texcoords.push_back(t.x); texcoords.push_back(t.y); texcoords.push_back(0.0f); texcoords.push_back(1.0f);
            } else { texcoords.insert(texcoords.end(), {0.f,0.f,0.f,1.f}); }

            if (mesh->HasTangentsAndBitangents()) {
                const aiVector3D &tg = mesh->mTangents[i];
                tangents.push_back(tg.x); tangents.push_back(tg.y); tangents.push_back(tg.z); tangents.push_back(1.0f);
            } else { tangents.insert(tangents.end(), {1.f,0.f,0.f,1.f}); }
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace &face = mesh->mFaces[f];
            if (face.mNumIndices == 3) {
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }
        }

        MyMesh amesh{};
        amesh.numIndexes = static_cast<GLuint>(indices.size());

        GLuint vbo[2];
        glGenVertexArrays(1, &amesh.vao);
        glBindVertexArray(amesh.vao);
        glGenBuffers(2, vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
        size_t vertsSize = vertices.size()*sizeof(float);
        size_t normsSize = normals.size()*sizeof(float);
        size_t uvsSize   = texcoords.size()*sizeof(float);
        size_t tangSize  = tangents.size()*sizeof(float);
        glBufferData(GL_ARRAY_BUFFER, vertsSize+normsSize+uvsSize+tangSize, nullptr, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertsSize, vertices.data());
        glBufferSubData(GL_ARRAY_BUFFER, vertsSize, normsSize, normals.data());
        glBufferSubData(GL_ARRAY_BUFFER, vertsSize+normsSize, uvsSize, texcoords.data());
        glBufferSubData(GL_ARRAY_BUFFER, vertsSize+normsSize+uvsSize, tangSize, tangents.data());

        glEnableVertexAttribArray(Shader::VERTEX_COORD_ATTRIB);
        glVertexAttribPointer(Shader::VERTEX_COORD_ATTRIB, 4, GL_FLOAT, 0, 0, 0);
        glEnableVertexAttribArray(Shader::NORMAL_ATTRIB);
        glVertexAttribPointer(Shader::NORMAL_ATTRIB, 4, GL_FLOAT, 0, 0, (void*)vertsSize);
        glEnableVertexAttribArray(Shader::TEXTURE_COORD_ATTRIB);
        glVertexAttribPointer(Shader::TEXTURE_COORD_ATTRIB, 4, GL_FLOAT, 0, 0, (void*)(vertsSize+normsSize));
        glEnableVertexAttribArray(Shader::TANGENT_ATTRIB);
        glVertexAttribPointer(Shader::TANGENT_ATTRIB, 4, GL_FLOAT, 0, 0, (void*)(vertsSize+normsSize+uvsSize));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[1]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);

        amesh.type = GL_TRIANGLES;

        // material
        if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials) {
            fillMaterialFromAi(scene->mMaterials[mesh->mMaterialIndex], amesh.mat);
        } else {
            float amb[4] = {0.2f,0.2f,0.2f,1.0f};
            float dif[4] = {0.8f,0.8f,0.8f,1.0f};
            float spe[4] = {0.2f,0.2f,0.2f,1.0f};
            memcpy(amesh.mat.ambient, amb, sizeof(amb));
            memcpy(amesh.mat.diffuse, dif, sizeof(dif));
            memcpy(amesh.mat.specular, spe, sizeof(spe));
            amesh.mat.shininess = 32.0f;
            amesh.mat.texCount = 0;
        }

        myMeshes.push_back(amesh);
        outMeshCount++;
    }

    return firstIndex;
}