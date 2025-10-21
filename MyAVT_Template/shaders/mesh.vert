#version 430

// ---------- Matrices ----------
uniform mat4 m_pvm;
uniform mat4 m_viewModel;
uniform mat4 m_Model;           // used to transform the unit cube for the skybox
uniform mat3 m_normal;
uniform mat4 m_View;            // needed to move reflect() vec into world space

// ---------- Controls (mirrors the demo) ----------
uniform int  texMode;           // 0=cube env map, 2=normal map, 3=skybox, 4=sphere env map
uniform int  reflect_perFrag;   // when 0, compute reflected vec here; when 1, in fragment

// ---------- Legacy inputs ----------
uniform vec4 l_pos;             // light pos in eye-space (legacy path)
uniform bool normalMap;         // kept for compatibility with your pipeline

// ---------- Vertex Attributes ----------
in vec4 position;
in vec4 normal;
in vec4 texCoord;
in vec4 tangent;
in vec4 bitangent;              // may be unused by the demo but we’ll keep it

// ---------- Varyings ----------
out Data {
    // existing stuff you already output
    vec3 normal;        // eye-space
    vec3 eye;           // eye-space view vector (from frag to eye)
    vec3 lightDir;      // eye-space light dir
    vec2 tex_coord;
    vec3 fragPos;       // eye-space position (kept since you had it)
    mat3 TBN;           // eye-space tangent basis
    vec3 skyboxTexCoord; 
} DataOut;

void main() {
    // ----- Positions -----
    vec4 posEye = m_viewModel * position;   // to eye space
    DataOut.fragPos = vec3(posEye);

    // ----- Build eye-space basis (robust) -----
    vec3 N = normalize(m_normal * normal.xyz);
    vec3 T = normalize(m_normal * tangent.xyz);
    // Re-orthogonalize T to N to avoid skew
    T = normalize(T - N * dot(T, N));
    // Prefer provided bitangent if valid, otherwise derive
    vec3 B = normalize(m_normal * bitangent.xyz);
    if (length(B) < 1e-4) {
        B = normalize(cross(N, T));
    }
    DataOut.normal = N;
    DataOut.TBN    = mat3(T, B, N);       // columns T,B,N (tangent→eye)

    // ----- Eye and light in eye space -----
    vec3 eyeDir   = -vec3(posEye);        // eye at origin in eye-space
    vec3 lightDir = vec3(l_pos - posEye);
    DataOut.eye       = eyeDir;           // not normalized (demo normalizes in frag)
    DataOut.lightDir  = normalize(lightDir);

    // ----- Base texcoords -----
    DataOut.tex_coord = texCoord.st;

    // ----- SKYBOX: cube coords in world/model space (like the demo) -----
    // Transform the unit cube vertex by m_Model and flip X (textures mapped inside)
    DataOut.skyboxTexCoord  = vec3(m_Model * position);
    DataOut.skyboxTexCoord.x = -DataOut.skyboxTexCoord.x;

    // ----- Final position -----
    gl_Position = m_pvm * position;
}
