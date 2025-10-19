#version 430

out vec4 colorOut;

uniform sampler2D texUnitDiff0;
uniform sampler2D texUnitDiff1;
uniform sampler2D texUnitSpec;
uniform sampler2D texUnitNormal;

uniform int  diffMapCount;   // number of diffuse texture samplers
uniform bool normalMap;      // enable normal mapping
uniform bool specularMap;    // enable specular map

struct Materials {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    int   texCount;
};
uniform Materials mat;

uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;
uniform sampler2D texmap3;
uniform vec3 fogColor;
uniform float fogDensity;
uniform int texMode;
uniform bool night_mode;
uniform bool plight_mode;
uniform bool headlights_mode;
uniform bool fog_mode;
uniform float spotCosCutOff;
uniform bool is_Hud;
uniform vec4 uHudColor;
uniform float uAlpha;
uniform bool uIsImportedModel;
uniform vec2 terrainTile1;
uniform vec2 terrainTile2;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
uniform DirLight   dirLight;
uniform PointLight pointLights[7];
uniform SpotLight  spotLights[4];
uniform int        numPointLights;
uniform int        numSpotLights;

in Data {
    vec3 normal;
    vec3 eye;
    vec3 lightDir;
    vec2 tex_coord;
    vec3 fragPos;
    mat3 TBN;
} DataIn;

void main() {
    vec2 TexCoord = DataIn.tex_coord;

    vec4 spec = vec4(0.0);
    vec3 n;
    if (normalMap) {
        n = normalize(2.0 * texture(texUnitNormal, TexCoord).rgb - 1.0);
    } else {
        n = normalize(DataIn.normal);
    }

    vec3 l = normalize(DataIn.lightDir);
    vec3 e = normalize(DataIn.eye);

    float intensity = max(dot(n, l), 0.0);

    vec4 diff;
    vec4 auxSpec;
    if (mat.texCount == 0) {
        diff = mat.diffuse;
        auxSpec = mat.specular;
    } else {
        if (diffMapCount == 0) {
            diff = mat.diffuse;
        } else if (diffMapCount == 1) {
            diff = mat.diffuse * texture(texUnitDiff0, TexCoord);
        } else {
            diff = mat.diffuse * texture(texUnitDiff0, TexCoord) * texture(texUnitDiff1, TexCoord);
        }
        if (specularMap) {
            auxSpec = mat.specular * texture(texUnitSpec, TexCoord);
        } else {
            auxSpec = mat.specular;
        }
    }

    if (intensity > 0.0) {
        vec3 h = normalize(l + e);
        float intSpec = max(dot(h, n), 0.0);
        spec = auxSpec * pow(intSpec, mat.shininess);
    }

    colorOut = vec4((max(intensity * diff, diff * 0.15) + spec).rgb, 1.0);
}