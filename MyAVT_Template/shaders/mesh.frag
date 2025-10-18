#version 430

// ---------- Outputs ----------
out vec4 colorOut;

// ---------- Existing diffuse samplers (keep these) ----------
uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;
uniform sampler2D texmap3;

// ---------- Imported model-specific flags ----------
uniform int  diffMapCount;       // 0 = none, 1 = texUnitDiff0, 2 = texUnitDiff0 * texUnitDiff1
uniform bool normalMap;
uniform bool specularMap;

// ---------- Imported model-specific samplers ----------
uniform sampler2D texUnitDiff0;
uniform sampler2D texUnitDiff1;
uniform sampler2D texUnitSpec;
uniform sampler2D texUnitNormal;

// ---------- Fog ----------
uniform vec3  fogColor;     // Color of the fog
uniform float fogDensity;   // Density of the fog (set by renderer)

// ---------- Material ----------
struct Materials {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    int   texCount;
};
uniform Materials mat;

// ---------- Inputs ----------
in Data {
    vec3 normal;
    vec3 eye;
    vec3 lightDir;   // legacy single light (eye-space) or tangent-space when using normal map
    vec2 tex_coord;
    vec3 fragPos;    // eye-space frag pos
    mat3 TBN;
} DataIn;

// ---------- Modes / misc ----------
uniform int   texMode;
uniform bool  night_mode;
uniform bool  plight_mode;
uniform bool  headlights_mode;
uniform bool  fog_mode;
uniform float spotCosCutOff;
uniform bool  is_Hud;
uniform vec4  uHudColor;
uniform float uAlpha;

// NOTE: this was referenced but not declared in your snippet
uniform bool  uIsImportedModel;

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

uniform DirLight   dirLight;
uniform PointLight pointLights[7];
uniform SpotLight  spotLights[4];
uniform int        numPointLights;
uniform int        numSpotLights;

// ---------- Tiling ----------
uniform vec2 terrainTile1;
uniform vec2 terrainTile2;

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
    // ---------------- HUD passthrough ----------------
    if (is_Hud) {
        colorOut = uHudColor;
        return;
    }

    if (true) {
        vec3 n = normalize(DataIn.normal);
        vec3 viewDir = normalize(DataIn.eye); // eye-space view vector (from frag toward eye at origin)
        vec3 fragPos = DataIn.fragPos;

        vec3 result = vec3(0.0);


        // directional light
        if (!night_mode && !is_Hud) result += CalcDirLight(dirLight, n, viewDir);

        // point lights (use either numPointLights or 7)
        int pcount = (numPointLights > 0) ? numPointLights : 7;
        pcount = min(pcount, 7);
        if (plight_mode && !is_Hud) {
            for (int i = 0; i < pcount; ++i) {
                result += CalcPointLight(pointLights[i], n, fragPos, viewDir);
            }
        }

        // spot lights (use either numSpotLights or 4)
        int scount = (numSpotLights > 0) ? numSpotLights : 4;
        scount = min(scount, 4);
        if (headlights_mode && !is_Hud) {
            for (int i = 0; i < scount; ++i) {
                result += CalcSpotLight(spotLights[i], n, fragPos, viewDir);
            }
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
        float fogDensity = (fog_mode) ? 0.01f : 0.00;
        float dist = length(DataIn.eye);  // eye-space distance to camera
        float fogFactor = exp(-pow(fogDensity * dist, 2.0));
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        colorOut.rgb = mix(fogColor, colorOut.rgb, fogFactor);

        return;
    } else {
        // ===== Imported model path (fixed names/types) =====
        vec3 n;
        if (normalMap) {
            // Normal from normal map (tangent space)
            n = normalize(2.0 * texture(texUnitNormal, DataIn.tex_coord).rgb - 1.0);
        } else {
            n = normalize(DataIn.normal);
        }

        // In this path, lightDir & eye may be in tangent space already
        vec3 l = normalize(DataIn.lightDir);
        vec3 e = normalize(DataIn.eye);

        float intensity = max(dot(n, l), 0.0);

        vec4 diff;
        vec4 auxSpec;

        if (mat.texCount == 0) {
            diff    = mat.diffuse;
            auxSpec = mat.specular;
        } else {
            // Diffuse maps
            if (diffMapCount == 0) {
                diff = mat.diffuse;
            } else if (diffMapCount == 1) {
                diff = mat.diffuse * texture(texUnitDiff0, DataIn.tex_coord);
            } else {
                diff = mat.diffuse
                       * texture(texUnitDiff0, DataIn.tex_coord)
                       * texture(texUnitDiff1, DataIn.tex_coord);
            }

            // Specular map (optional)
            if (specularMap) {
                auxSpec = mat.specular * texture(texUnitSpec, DataIn.tex_coord);
            } else {
                auxSpec = mat.specular;
            }
        }

        vec4 spec = vec4(0.0);
        if (intensity > 0.0) {
            vec3 h      = normalize(l + e);
            float sdot  = max(dot(h, n), 0.0);
            float sPow  = pow(sdot, mat.shininess);
            spec        = auxSpec * sPow;
        }

        vec3 base = max(intensity * diff.rgb, diff.rgb * 0.15);
        vec4 col  = vec4(base + spec.rgb, 1.0);

        // Optional: apply same fog to imported path for consistency
        if (fog_mode) {
            float dist      = length(DataIn.eye);
            float fogFactor = exp(-pow(fogDensity * dist, 2.0));
            fogFactor       = clamp(fogFactor, 0.0, 1.0);
            col.rgb         = mix(fogColor, col.rgb, fogFactor);
        }

        colorOut = col;
        return;
    }
}

