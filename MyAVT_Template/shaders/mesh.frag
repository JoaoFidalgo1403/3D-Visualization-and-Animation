#version 430

// ---------- Outputs ----------
out vec4 colorOut;

// ---------- Existing diffuse samplers (keep these) ----------
uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;
uniform sampler2D texmap3;
uniform samplerCube cubeMap;

// ---------- Imported model-specific flags ----------
uniform uint  diffMapCount;       // 0 = none, 1 = texUnitDiff0, 2 = texUnitDiff0 * texUnitDiff1
uniform bool  normalMap;
uniform bool  specularMap;
uniform bool  emissiveMap;     


// ---------- Imported model-specific samplers ----------
uniform sampler2D texUnitDiff0;
uniform sampler2D texUnitDiff1;
uniform sampler2D texUnitSpec;
uniform sampler2D texUnitNormalMap;
uniform sampler2D texUnitEmissive;    // bind emissive texture here

// ---------- Fog ----------
uniform vec3  fogColor;     // Color of the fog
uniform float fogDensity;   // Density of the fog (set by renderer)

// ---------- Material ----------
struct Materials {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;   // Ke
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
    mat3 TBN;        // tangent->eye (or tangent->world) basis
	vec3 skyboxTexCoord;
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

// ---------- Lighting helpers (use mat.*) ----------
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
    vec3 L = normalize(-light.direction);
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
    vec3 L = normalize(light.position - fragPos);
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
    vec3 LtoFrag = normalize(fragPos - light.position);
    float cosTheta = dot(LtoFrag, normalize(light.direction));
    if (cosTheta <= light.outerCutOff) return vec3(0.0);

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
    vec3 diffuse  = light.diffuse * (diff * mat.diffuse.rgb) * attenuation * intensity;
    vec3 specular = light.specular * (specFactor * mat.specular.rgb) * attenuation * intensity;

    return ambient + diffuse + specular;
}

// ---------- Parameterized helpers (for imported models using their textures) ----------
vec3 CalcDirLight_M(DirLight light, vec3 n, vec3 v, vec3 ambC, vec3 diffC, vec3 specC, float shininess) {
    vec3 L = normalize(-light.direction);
    float diff = max(dot(n, L), 0.0);
    vec3 H = normalize(L + v);
    float s = (diff > 0.0) ? pow(max(dot(n, H), 0.0), shininess) : 0.0;

    vec3 ambient  = light.ambient * ambC;
    vec3 diffuse  = light.diffuse * (diff * diffC);
    vec3 specular = light.specular * (s    * specC);
    return ambient + diffuse + specular;
}

vec3 CalcPointLight_M(PointLight light, vec3 n, vec3 p, vec3 v, vec3 ambC, vec3 diffC, vec3 specC, float shininess) {
    vec3 L = normalize(light.position - p);
    float diff = max(dot(n, L), 0.0);
    vec3 H = normalize(L + v);
    float s = (diff > 0.0) ? pow(max(dot(n, H), 0.0), shininess) : 0.0;

    float d = length(light.position - p);
    float att = 1.0 / (light.constant + light.linear * d + light.quadratic * d * d);

    vec3 ambient  = light.ambient * ambC * att;
    vec3 diffuse  = light.diffuse * (diff * diffC) * att;
    vec3 specular = light.specular * (s    * specC) * att;
    return ambient + diffuse + specular;
}

vec3 CalcSpotLight_M(SpotLight light, vec3 n, vec3 p, vec3 v, vec3 ambC, vec3 diffC, vec3 specC, float shininess) {
    vec3 LtoFrag = normalize(p - light.position);
    float cosTheta = dot(LtoFrag, normalize(light.direction));
    if (cosTheta <= light.outerCutOff) return vec3(0.0);

    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((cosTheta - light.outerCutOff) / max(epsilon, 1e-6), 0.0, 1.0);

    vec3 L = normalize(light.position - p);
    float diff = max(dot(n, L), 0.0);
    vec3 H = normalize(L + v);
    float s = (diff > 0.0) ? pow(max(dot(n, H), 0.0), shininess) : 0.0;

    float d = length(light.position - p);
    float att = 1.0 / (light.constant + light.linear * d + light.quadratic * d * d);

    vec3 ambient  = light.ambient * ambC * att * intensity;
    vec3 diffuse  = light.diffuse * (diff * diffC) * att * intensity;
    vec3 specular = light.specular * (s    * specC) * att * intensity;
    return ambient + diffuse + specular;
}

vec4 diff;
vec4 auxSpec;

void main() {
    // ---------------- HUD passthrough ----------------
    if (is_Hud) {
        colorOut = uHudColor;
        return;
    }

    vec4 finalColor = vec4(0.0);  // local accumulator

    if (!uIsImportedModel) {
        // ===== Original path (unchanged lighting) =====
        vec3 n = normalize(DataIn.normal);
        vec3 viewDir = normalize(DataIn.eye);
        vec3 fragPos = DataIn.fragPos;

        vec3 result = vec3(0.0);

        if (!night_mode) result += CalcDirLight(dirLight, n, viewDir);

        int pcount = (numPointLights > 0) ? numPointLights : 7;
        pcount = min(pcount, 7);
        if (plight_mode) {
            for (int i = 0; i < pcount; ++i) {
                result += CalcPointLight(pointLights[i], n, fragPos, viewDir);
            }
        }

        int scount = (numSpotLights > 0) ? numSpotLights : 4;
        scount = min(scount, 4);
        if (headlights_mode) {
            for (int i = 0; i < scount; ++i) {
                result += CalcSpotLight(spotLights[i], n, fragPos, viewDir);
            }
        }

        // Compose final color depending on texturing mode.
        if (texMode == 0) {
            finalColor = vec4(clamp(result + mat.emissive.rgb, 0.0, 1.0), uAlpha);
        } else if (texMode == 1) {
            vec3 texel = texture(texmap, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            finalColor = vec4(outc, uAlpha);
        } else if (texMode == 2) {
            vec3 texel = texture(texmap1, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            finalColor = vec4(outc, uAlpha);
        } else if (texMode == 3) {
            vec3 texel = texture(texmap2, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            finalColor = vec4(outc, uAlpha);
        } else if (texMode == 4) {
            vec3 texel = texture(texmap3, DataIn.tex_coord).rgb;
            vec3 outc = clamp(result * texel + 0.07 * texel, 0.0, 1.0);
            finalColor = vec4(outc, uAlpha);
        } else if (texMode == 5) { // Billboards
            vec4 texel = texture(texmap3, DataIn.tex_coord);
            vec3 outc = clamp(result * texel.rgb + 0.07 * texel.rgb, 0.0, 1.0);
            if (texel.a < 0.40) discard;
            else finalColor = vec4(outc, texel.a);
        } else if (texMode == 6) { // Particles
            vec4 texel = texture(texmap3, DataIn.tex_coord);
            if ((texel.a < 0.20) || (mat.diffuse.a == 0.0)) discard;
            else finalColor = vec4(mat.diffuse.rgb, texel.a);
        } else if (texMode == 7) {
            vec2 tiledTC1 = DataIn.tex_coord * terrainTile1;
            vec2 tiledTC2 = DataIn.tex_coord * terrainTile2;
            vec3 texel  = texture(texmap2, tiledTC2).rgb;
            vec3 texel1 = texture(texmap1, tiledTC1).rgb;
            vec3 outc = clamp(result * texel * texel1 + 0.07 * texel * texel1, 0.0, 1.0);
            finalColor = vec4(outc, uAlpha);
        } else if (texMode == 8) {
            finalColor = texture(cubeMap, DataIn.skyboxTexCoord);
        } else {
            vec4 texel = texture(texmap, DataIn.tex_coord);  //texel from element flare texture
            if((texel.a == 0.0)  || (mat.diffuse.a == 0.0) ) discard;
            else finalColor = mat.diffuse * texel;
        }

    } else {
        // ===== Imported model path (now using emissive map) =====

        // ----- Normal (object or normal-mapped via TBN) -----
        vec3 n;
        if (normalMap) {
            // normal map in tangent space -> eye space using TBN
            vec3 n_ts = 2.0 * texture(texUnitNormalMap, DataIn.tex_coord).rgb - 1.0;
            n = normalize(DataIn.TBN * n_ts);
        } else {
            n = normalize(DataIn.normal);
        }

        vec3 v = normalize(DataIn.eye);   // view dir (eye space)
        vec3 p = DataIn.fragPos;          // frag position (eye space)

        // ----- Build material colors from imported textures -----
        // Diffuse/albedo
        vec3 albedo = mat.diffuse.rgb;
        if (diffMapCount == 1u) {
            albedo *= texture(texUnitDiff0, DataIn.tex_coord).rgb;
        } else if (diffMapCount >= 2u) {
            albedo *= texture(texUnitDiff0, DataIn.tex_coord).rgb
                    * texture(texUnitDiff1, DataIn.tex_coord).rgb;
        }

        // Specular color (allow map to scale/tint spec highlights)
        vec3 specC = mat.specular.rgb;
        if (specularMap) {
            specC *= texture(texUnitSpec, DataIn.tex_coord).rgb;
        }

        // Ambient color — usually albedo-tinted so dark areas don’t glow
        vec3 ambC = mat.ambient.rgb * albedo;
        float shin = mat.shininess;

        // ----- Accumulate lights exactly like the other path -----
        vec3 result = vec3(0.0);

        if (!night_mode) {
            result += CalcDirLight_M(dirLight, n, v, ambC, albedo, specC, shin);
        }

        int pcount = (numPointLights > 0) ? numPointLights : 7;
        pcount = min(pcount, 7);
        if (plight_mode) {
            for (int i = 0; i < pcount; ++i) {
                result += CalcPointLight_M(pointLights[i], n, p, v, ambC, albedo, specC, shin);
            }
        }

        int scount = (numSpotLights > 0) ? numSpotLights : 4;
        scount = min(scount, 4);
        if (headlights_mode) {
            for (int i = 0; i < scount; ++i) {
                result += CalcSpotLight_M(spotLights[i], n, p, v, ambC, albedo, specC, shin);
            }
        }

        // ----- Emissive: map_Ke modulates Ke per-pixel -----
        vec3 emissiveC = mat.emissive.rgb;
        if (emissiveMap) {
            vec3 emTex = texture(texUnitEmissive, DataIn.tex_coord).rgb;
            // If emissive textures are stored in sRGB and not sampled from an sRGB texture,
            // uncomment the next line to linearize:
            // emTex = pow(emTex, vec3(2.2));
            emissiveC *= emTex;
        }

        // Output (emission is added to lit result)
        vec3 finalRGB = clamp(result + emissiveC, 0.0, 1.0);
        finalColor = vec4(finalRGB, uAlpha);
    }

    // Fog (same treatment)
    vec3 fogC = vec3(0.35, 0.18, 0.08);
    float fDen = (fog_mode) ? 0.015 : 0.0;
    float dist = length(DataIn.eye);
    float fogFactor = clamp(exp(-pow(fDen * dist, 2.0)), 0.0, 1.0);
    finalColor.rgb = mix(fogC, finalColor.rgb, fogFactor);

    colorOut = finalColor;

    return;
}
