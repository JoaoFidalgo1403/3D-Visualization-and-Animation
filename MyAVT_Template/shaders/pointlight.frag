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


vec4 diff, auxSpec;

void main() {

	vec4 spec = vec4(0.0);
	vec3 n;

	if(normalMap)
		n = normalize(2.0 * texture(texUnitNormal, DataIn.tex_coord).rgb - 1.0);  //normal in tangent space
	else
		n = normalize(DataIn.normal);

	//If bump mapping, normalMap == TRUE, then lightDir and eye vectores come from vertex shader in tangent space
	vec3 l = normalize(DataIn.lightDir);
	vec3 e = normalize(DataIn.eye);

	float intensity = max(dot(n,l), 0.0);

	if(mat.texCount == 0) {
		diff = mat.diffuse;
		auxSpec = mat.specular;
	}
	else {
		if(diffMapCount == 0)
			diff = mat.diffuse;
		else if(diffMapCount == 1)
			diff = mat.diffuse * texture(texUnitDiff0, DataIn.tex_coord);
		else
			diff = mat.diffuse * texture(texUnitDiff0, DataIn.tex_coord) * texture(texUnitDiff1, DataIn.tex_coord);

		if(specularMap) 
			auxSpec = mat.specular * texture(texUnitSpec, DataIn.tex_coord);
		else
			auxSpec = mat.specular;
	}

	if (intensity > 0.0) {
		vec3 h = normalize(l + e);
		float intSpec = max(dot(h,n), 0.0);
		spec = auxSpec * pow(intSpec, mat.shininess);
		
	}

	colorOut = vec4((max(intensity * diff, diff*0.15) + spec).rgb, 1.0);
}