#version 430

uniform mat4 m_pvm;
uniform mat4 m_viewModel;
uniform mat3 m_normal;

uniform vec4 l_pos;            // legacy single light position (eye-space)
uniform bool useNormalMap;     // match fragment's flag

in vec4 position;              // keep vec4 for backwards compatibility (app can provide vec3)
in vec4 normal;                // same
in vec4 texCoord;              // same
in vec4 tangent;               // optional: provide when using normal maps
in vec4 bitangent;             // optional: provide when using normal maps

out Data {
    vec3 normal;    // normal in eye space
    vec3 eye;       // vector from fragment to eye (eye at origin in eye space)
    vec3 lightDir;  // legacy single light direction (eye-space) or tangent-space when normal map enabled
    vec2 tex_coord;
    vec3 fragPos;   // fragment position in eye space
} DataOut;

void main() {
    // position in eye space
    vec4 posEye = m_viewModel * position;
    DataOut.fragPos = vec3(posEye);

    // eye-space normal
    vec3 n = normalize(m_normal * normal.xyz);
    DataOut.normal = n;

    // eye-space vectors
    vec3 eyeDir = -vec3(posEye);           // from fragment toward eye (eye at origin)
    vec3 lightDir = vec3(l_pos - posEye);  // from fragment toward light (eye-space)

    // if normal mapping is enabled, transform light/eye into tangent space
    if (useNormalMap) {
        vec3 t = normalize(m_normal * tangent.xyz);
        vec3 b = normalize(m_normal * bitangent.xyz);

        vec3 aux;
        aux.x = dot(lightDir, t);
        aux.y = dot(lightDir, b);
        aux.z = dot(lightDir, n);
        lightDir = normalize(aux);

        aux.x = dot(eyeDir, t);
        aux.y = dot(eyeDir, b);
        aux.z = dot(eyeDir, n);
        eyeDir = normalize(aux);
    }

    DataOut.lightDir = lightDir;
    DataOut.eye = eyeDir;

    DataOut.tex_coord = texCoord.st;

    gl_Position = m_pvm * position;
}
