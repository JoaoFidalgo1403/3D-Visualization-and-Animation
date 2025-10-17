#version 430

// ---------- Outputs ----------
out vec4 colorOut;

// ---------- Existing diffuse samplers (keep these) ----------
uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;
uniform sampler2D texmap3;

// ---------- New optional samplers ----------
uniform sampler2D texUnitSpec;   // optional specular map
uniform sampler2D texUnitNormal; // optional normal map (tangent-space)

// ---------- Fog ----------
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

// ---------- Material ----------
struct Materials {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    int texCount;
};
uniform Materials mat;

// ---------- Inputs ----------
in Data {
    vec3 normal;
    vec3 eye;
    vec3 lightDir;   // legacy single light (eye-space) or tangent-space when using normal map
    vec2 tex_coord;
    vec3 fragPos;    // eye-space frag pos
} DataIn;

// ---------- Modes / misc ----------
uniform int texMode;
uniform bool night_mode;
uniform bool plight_mode;
uniform bool headlights_mode;
uniform bool fog_mode;
uniform float spotCosCutOff;
uniform bool is_Hud;
uniform vec4 uHudColor;
uniform float uAlpha;

// ---------- New flags ----------
uniform bool useNormalMap;    // sample texUnitNormal (tangent-space)
uniform bool useSpecularMap;  // sample texUnitSpec
uniform int diffMapCount;     // 0 = none, 1 = texmap, 2 = texmap * texmap1

// ---------- Lights ----------
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

uniform DirLight dirLight;
uniform PointLight pointLights[7];
uniform SpotLight spotLights[4];
uniform int numPointLights;
uniform int numSpotLights;

// ---------- Tiling ----------
uniform vec2 terrainTile1;
uniform vec2 terrainTile2;

// ---------- Lighting helpers ----------
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffCol, vec3 specCol) {
    vec3 L = normalize(-light.direction);
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    vec3 ambient  = light.ambient * mat.ambient.rgb;
    vec3 diffuse  = light.diffuse * (diff * diffCol);
    vec3 specular = light.specular * (specFactor * specCol);
    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffCol, vec3 specCol) {
    vec3 L = normalize(light.position - fragPos);
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 ambient  = light.ambient * mat.ambient.rgb * attenuation;
    vec3 diffuse  = light.diffuse * (diff * diffCol) * attenuation;
    vec3 specular = light.specular * (specFactor * specCol) * attenuation;
    return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffCol, vec3 specCol) {
    vec3 LtoFrag = normalize(fragPos - light.position);
    float cosTheta = dot(LtoFrag, normalize(light.direction));
    if (cosTheta <= light.outerCutOff) {
        return vec3(0.0);
    }

    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((cosTheta - light.outerCutOff) / max(epsilon, 1e-6), 0.0, 1.0);

    vec3 L = normalize(light.position - fragPos);
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 ambient  = light.ambient * mat.ambient.rgb * attenuation * intensity;
    vec3 diffuse  = light.diffuse * (diff * diffCol) * attenuation * intensity;
    vec3 specular = light.specular * (specFactor * specCol) * attenuation * intensity;

    return ambient + diffuse + specular;
}

void main() {
    // HUD early-out
    if (is_Hud) {
        colorOut = uHudColor;
        return;
    }

    // Determine normal and viewDir
    vec3 n;
    vec3 viewDir;
    if (useNormalMap) {
        // tangent-space normal assumed; vertex shader must supply light/eye in tangent-space or provide TBN
        n = normalize(2.0 * texture(texUnitNormal, DataIn.tex_coord).rgb - 1.0);
        viewDir = normalize(DataIn.eye); // expect tangent-space when using normal map
    } else {
        n = normalize(DataIn.normal);
        viewDir = normalize(DataIn.eye);
    }

    // Build diffuseColor: diffMapCount overrides texMode behavior when > 0
    vec3 diffuseColor = mat.diffuse.rgb;
    if (diffMapCount == 1) {
        diffuseColor = mat.diffuse.rgb * texture(texmap, DataIn.tex_coord).rgb;
    } else if (diffMapCount >= 2) {
        vec3 d0 = texture(texmap, DataIn.tex_coord).rgb;
        vec3 d1 = texture(texmap1, DataIn.tex_coord).rgb;
        diffuseColor = mat.diffuse.rgb * d0 * d1;
    }

    // Specular modulation
    vec3 specColor = mat.specular.rgb;
    if (useSpecularMap) {
        specColor *= texture(texUnitSpec, DataIn.tex_coord).rgb;
    }

    // Accumulate lighting using diffuseColor/specColor
    vec3 result = vec3(0.0);
    if (!night_mode) result += CalcDirLight(dirLight, n, viewDir, diffuseColor, specColor);

    int pcount = (numPointLights > 0) ? numPointLights : 7;
    pcount = min(pcount, 7);
    if (plight_mode) {
        for (int i = 0; i < pcount; ++i) {
            result += CalcPointLight(pointLights[i], n, DataIn.fragPos, viewDir, diffuseColor, specColor);
        }
    }

    int scount = (numSpotLights > 0) ? numSpotLights : 4;
    scount = min(scount, 4);
    if (headlights_mode) {
        for (int i = 0; i < scount; ++i) {
            result += CalcSpotLight(spotLights[i], n, DataIn.fragPos, viewDir, diffuseColor, specColor);
        }
    }

    // Final color composition:
    // If diffMapCount > 0 we already composed diffuseColor from texmap/texmap1.
    // Otherwise follow original texMode behavior for single-texture modulation.
    vec3 outc;
    if (diffMapCount > 0) {
        outc = clamp(result * diffuseColor + 0.07 * diffuseColor + mat.emissive.rgb, 0.0, 1.0);
    } else {
        if (texMode == 0) {
            outc = clamp(result + mat.emissive.rgb, 0.0, 1.0);
        } else if (texMode == 1) {
            vec3 texel = texture(texmap, DataIn.tex_coord).rgb;
            outc = clamp(result * texel + 0.07 * texel + mat.emissive.rgb, 0.0, 1.0);
        } else if (texMode == 2) {
            vec3 texel = texture(texmap1, DataIn.tex_coord).rgb;
            outc = clamp(result * texel + 0.07 * texel + mat.emissive.rgb, 0.0, 1.0);
        } else if (texMode == 3) {
            vec3 texel = texture(texmap2, DataIn.tex_coord).rgb;
            outc = clamp(result * texel + 0.07 * texel + mat.emissive.rgb, 0.0, 1.0);
        } else if (texMode == 4) {
            vec3 texel = texture(texmap3, DataIn.tex_coord).rgb;
            outc = clamp(result * texel + 0.07 * texel + mat.emissive.rgb, 0.0, 1.0);
        } else {
            vec2 tiledTC1 = DataIn.tex_coord * terrainTile1;
            vec2 tiledTC2 = DataIn.tex_coord * terrainTile2;
            vec3 texel = texture(texmap2, tiledTC2).rgb;
            vec3 texel1 = texture(texmap1, tiledTC1).rgb;
            vec3 combined = texel * texel1;
            outc = clamp(result * combined + 0.07 * combined + mat.emissive.rgb, 0.0, 1.0);
        }
    }

    colorOut = vec4(outc, uAlpha);

    // Fog (exponential, same behavior as before)
    vec3 fallbackFog = vec3(0.35, 0.18, 0.08);
    vec3 effectiveFog = (length(fogColor) < 1e-5) ? fallbackFog : fogColor;
    float fogDensity = (fog_mode) ? 0.01 : 0.0;
    float dist = length(DataIn.eye);
    float fogFactor = exp(-pow(fogDensity * dist, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    colorOut.rgb = mix(effectiveFog, colorOut.rgb, fogFactor);
}
