#version 430

uniform mat4 m_pvm;
uniform mat4 m_viewModel;
uniform mat3 m_normal;

uniform vec4 l_pos;        // legacy single light (eye-space)
uniform bool normalMap;

in vec4 position;
in vec4 normal;
in vec4 texCoord;
in vec4 tangent;
in vec4 bitangent;

out Data {
    vec3 normal;       // eye-space normal
    vec3 eye;          // eye-space view dir (from frag toward eye)
    vec3 lightDir;     // eye-space dir to legacy single light (kept for compatibility)
    vec2 tex_coord;
    vec3 fragPos;      // eye-space fragment pos
    mat3 TBN;          // NEW: eye-space TBN basis
} DataOut;

void main() {
    vec4 posEye = m_viewModel * position;
    DataOut.fragPos = vec3(posEye);

    // Build eye-space basis
    vec3 N = normalize(m_normal * normal.xyz);
    vec3 T = normalize(m_normal * tangent.xyz);
    // Re-orthogonalize T to N to avoid skew
    T = normalize(T - N * dot(T, N));
    // Prefer given bitangent if valid, otherwise derive from cross
    vec3 B = normalize(m_normal * bitangent.xyz);
    if (length(B) < 1e-4) {
        B = normalize(cross(N, T));
    }

    DataOut.normal = N;
    DataOut.TBN    = mat3(T, B, N);   // columns: T, B, N (tangent space → eye space)

    // Eye/legacy light in eye space (no tangent conversion here)
    vec3 eyeDir   = -vec3(posEye);
    vec3 lightDir = vec3(l_pos - posEye);

    DataOut.eye       = normalize(eyeDir);
    DataOut.lightDir  = normalize(lightDir);
    DataOut.tex_coord = texCoord.st;

    gl_Position = m_pvm * position;
}
