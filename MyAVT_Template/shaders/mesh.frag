#version 430

// ---------- Fog ----------
uniform vec3 fogColor;
uniform float fogStart;  // distance where fog starts
uniform float fogEnd;    // distance where fog is full

// ---------- Material ----------
struct Materials {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    int texCount;
};

in Data {
    vec3 normal;
    vec3 eye;
    vec3 lightDir;   // legacy single light (eye-space)
    vec2 tex_coord;
    vec3 fragPos;    // eye-space frag pos
} DataIn;

uniform Materials mat;

// ---------- Textures ----------
uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;
uniform sampler2D texmap3;

uniform int texMode;
uniform bool spotlight_mode;    // legacy toggle (keeps old behaviour)
uniform vec4 coneDir;           // legacy spot direction (eye-space)
uniform float spotCosCutOff;    // legacy cut-off cosine

out vec4 colorOut;

// ---------- New Light Structs ----------
struct DirLight {
    vec3 direction;   // direction of the light (eye-space) - direction of light rays
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;    // eye-space
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;    // eye-space
    vec3 direction;   // normalized direction the spotlight points toward (eye-space)
    float cutOff;     // cosine of inner angle
    float outerCutOff;// cosine of outer angle
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

// ---------- Transparency ----------

uniform float uAlpha; // 1.0 by default (opaque)


// ---------- Tiling ----------
uniform vec2 terrainTile1; // (1,1) default — multiply texcoords to tile
uniform vec2 terrainTile2; // (1,1) default — multiply texcoords to tile

// ---------- Lighting helpers ----------
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    // light.direction is the direction of light rays (e.g. (0,-1,0) points downward)
    vec3 L = normalize(-light.direction); // vector from fragment toward light
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    vec3 ambient  = light.ambient * mat.ambient.rgb;
    vec3 diffuse  = light.diffuse * (diff * mat.diffuse.rgb);
    vec3 specular = light.specular * (specFactor * mat.specular.rgb);
    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 L = normalize(light.position - fragPos); // from frag to light
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 ambient  = light.ambient * mat.ambient.rgb * attenuation;
    vec3 diffuse  = light.diffuse * (diff * mat.diffuse.rgb) * attenuation;
    vec3 specular = light.specular * (specFactor * mat.specular.rgb) * attenuation;
    return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 LtoFrag = normalize(fragPos - light.position); // vector pointing from light to fragment
    float cosTheta = dot(LtoFrag, normalize(light.direction)); // high when inside cone

    if (cosTheta <= light.outerCutOff) {
        // outside outer cone -> no contribution
        return vec3(0.0);
    }

    // smoothstep between outer and inner cones
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((cosTheta - light.outerCutOff) / max(epsilon, 1e-6), 0.0, 1.0);

    // now compute standard point-light terms (using L from frag to light)
    vec3 L = normalize(light.position - fragPos);
    float diff = max(dot(normal, L), 0.0);
    vec3 H = normalize(L + viewDir);
    float specFactor = 0.0;
    if (diff > 0.0) specFactor = pow(max(dot(normal, H), 0.0), mat.shininess);

    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 ambient  = light.ambient * mat.ambient.rgb * attenuation * intensity;
    vec3 diffuse  = light.diffuse * (diff * mat.diffuse.rgb) * attenuation * intensity;
    vec3 specular = light.specular * (specFactor * mat.specular.rgb) * attenuation * intensity;

    return ambient + diffuse + specular;
}

void main() {
    vec3 n = normalize(DataIn.normal);
    vec3 viewDir = normalize(DataIn.eye); // eye-space view vector (from frag toward eye at origin)
    vec3 fragPos = DataIn.fragPos;

    // Legacy single-spotlight path (keeps old behaviour when spotlight_mode toggled)
    if (spotlight_mode) {
        // Use only the spotLights[] array (ignore dir + point lights)
        vec3 result = vec3(0.0);

        int scount = (numSpotLights > 0) ? numSpotLights : 4;
        scount = min(scount, 4);
        for (int i = 0; i < scount; ++i) {
            result += CalcSpotLight(spotLights[i], n, fragPos, viewDir);
        }

        // Compose final color depending on texturing mode (use `result` as lighting)
        if (texMode == 0) {
            // no texturing
            colorOut = vec4(clamp(result + mat.emissive.rgb, 0.0, 1.0), uAlpha);
        } else if (texMode == 1) {
            vec3 texel = texture(texmap, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            colorOut = vec4(outc, uAlpha);
        } else if (texMode == 2) {
            vec3 texel = texture(texmap1, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            colorOut = vec4(outc, uAlpha);
        } else if (texMode == 3) {
            vec3 texel = texture(texmap2, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            colorOut = vec4(outc, uAlpha);
        } else if (texMode == 4) {
            vec3 texel = texture(texmap3, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            colorOut = vec4(outc, uAlpha);
        } else {
            vec2 tiledTC1 = DataIn.tex_coord * terrainTile1;
            vec2 tiledTC2 = DataIn.tex_coord * terrainTile2;
            vec3 texel = texture(texmap2, tiledTC2).rgb;
            vec3 texel1 = texture(texmap1, tiledTC1).rgb;
            vec3 outc = clamp(result * texel * texel1 + 0.07 * texel * texel1, 0.0, 1.0);
            colorOut = vec4(outc, uAlpha);
        }

        return;
    }

    // ---------- Multi-light path (directional + point lights + spot lights) ----------
    vec3 result = vec3(0.0);

    // directional light
    result += CalcDirLight(dirLight, n, viewDir);

    // point lights (use either numPointLights or 7)
    int pcount = (numPointLights > 0) ? numPointLights : 7;
    pcount = min(pcount, 7);
    for (int i = 0; i < pcount; ++i) {
        result += CalcPointLight(pointLights[i], n, fragPos, viewDir);
    }

    // spot lights (use either numSpotLights or 4)
    int scount = (numSpotLights > 0) ? numSpotLights : 4;
    scount = min(scount, 4);
    for (int i = 0; i < scount; ++i) {
        result += CalcSpotLight(spotLights[i], n, fragPos, viewDir);
    }

    // Compose final color depending on texturing mode.
    if (texMode == 0) {
        // no texturing: result already includes material ambient/diffuse/specular
        colorOut = vec4(clamp(result + mat.emissive.rgb, 0.0, 1.0), uAlpha);
    } else if (texMode == 1) {
        vec3 texel = texture(texmap, DataIn.tex_coord).rgb;
        // modulate final lighting by texel (approximation of original behavior)
        vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
        colorOut = vec4(outc, uAlpha);
    } else if (texMode == 2) {
        vec3 texel = texture(texmap1, DataIn.tex_coord).rgb;
        vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
        colorOut = vec4(outc, uAlpha);
    } else if (texMode == 3) {
        vec3 texel = texture(texmap2, DataIn.tex_coord).rgb;
        vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
        colorOut = vec4(outc, uAlpha);
     } else if (texMode == 4) {
        vec3 texel = texture(texmap3, DataIn.tex_coord).rgb;
        vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
        colorOut = vec4(outc, uAlpha);
    } else {
        vec2 tiledTC1 = DataIn.tex_coord * terrainTile1;
        vec2 tiledTC2 = DataIn.tex_coord * terrainTile2;
        vec3 texel = texture(texmap2, tiledTC2).rgb;
        vec3 texel1 = texture(texmap1, tiledTC1).rgb;
        vec3 outc = clamp(result * texel * texel1 + 0.07 * texel * texel1, 0.0, 1.0);
        colorOut = vec4(outc, uAlpha);
    }

    // Apply fog at the very end of main()
    vec3 fogColor = vec3(0.35, 0.18, 0.08); // #5a2e14
    float fogDensity = 0.01;
    float dist = length(DataIn.eye);  // eye-space distance to camera
    float fogFactor = exp(-pow(fogDensity * dist, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    colorOut.rgb = mix(fogColor, colorOut.rgb, fogFactor);

    return;
}
