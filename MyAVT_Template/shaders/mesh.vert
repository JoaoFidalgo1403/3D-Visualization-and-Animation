#version 430

uniform mat4 m_pvm;
uniform mat4 m_viewModel;
uniform mat3 m_normal;

uniform vec4 l_pos; // legacy single light position (world->eye will be computed in vertex)

in vec4 position;
in vec4 normal;    // from geometry generator
in vec4 texCoord;

out Data {
    vec3 normal;    // normal in eye space
    vec3 eye;       // vector from fragment to eye (eye at origin in eye space)
    vec3 lightDir;  // legacy single light direction (eye-space)
    vec2 tex_coord;
    vec3 fragPos;   // fragment position in eye space
} DataOut;

void main () {

    // position in eye space
    vec4 posEye = m_viewModel * position;

    DataOut.fragPos = vec3(posEye);
    DataOut.normal = normalize(m_normal * normal.xyz);
    DataOut.lightDir = vec3(l_pos - posEye); // legacy single-light vector (eye-space)
    DataOut.eye = -vec3(posEye);             // eye at origin in eye-space
    DataOut.tex_coord = texCoord.st;

    gl_Position = m_pvm * position;
}
