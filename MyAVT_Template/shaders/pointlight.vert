#version 430

uniform mat4 m_pvm;
uniform mat4 m_viewModel;
uniform mat3 m_normal;

uniform vec4 l_pos;
uniform bool normalMap;

// ---- Attributes as vec4 (your project setup) ----
in vec3 position;
in vec3 normal;
in vec3 tangent;
in vec3 bitangent;
in vec2 texCoord;   // if your UVs are actually vec2, change this back to "in vec2 texCoord;"

// ---- VS → FS payload (names must match your fragment shader) ----
out Data {
    vec3 normal;
    vec3 eye;
    vec3 lightDir;
    vec2 tex_coord;
    vec3 fragPos;    // eye-space
    mat3 TBN;        // not used by your FS but keep signature consistent if needed
} DataOut;

void main()
{
    vec4 worldPosition = model * vec4(position, 1.0);
    FragPos = vec3(worldPosition);

    // Transform normal and tangent
    vec3 T = normalize(mat3(model) * tangent);
    vec3 B = normalize(mat3(model) * bitangent);
    vec3 N = normalize(mat3(model) * normal);
    TBN = mat3(T, B, N);

    TexCoord = texCoord;

    gl_Position = projection * view * worldPosition;
}
