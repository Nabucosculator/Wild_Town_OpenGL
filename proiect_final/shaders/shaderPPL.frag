#version 410 core

in vec3 fragPosEye;
in vec3 fragNormalEye;
in vec2 fragTexCoords;
in vec4 fragPosLightSpace;

uniform vec3 lightDir;     // directional, eye space
uniform vec3 lightColor;

uniform vec3 baseColor;
uniform sampler2D diffuseTexture;
uniform int hasDiffuseTex;

// =========================
// FOG
// =========================
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform int fogEnabled; // 0/1

// =========================
// MULTI POINT LIGHTS
// =========================
#define MAX_POINT_LIGHTS 8
uniform int  numPointLights;
uniform vec3 pointLightPosEye[MAX_POINT_LIGHTS];
uniform vec3 pointLightColor[MAX_POINT_LIGHTS];

// =========================
// SHADOWS (NEW)
// =========================
uniform sampler2D shadowMap; // depth map
uniform int enableShadows;   // 0/1

out vec4 fColor;

float ShadowFactor(vec4 fragPosLS, vec3 N, vec3 Ld)
{
    // perspective divide
    vec3 projCoords = fragPosLS.xyz / fragPosLS.w;
    // to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // daca e in afara shadow map (xy), nu umbrim
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // daca e dincolo de far plane-ul luminii
    if (projCoords.z > 1.0) return 0.0;
    if (projCoords.z < 0.0) return 0.0;



    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    // bias (reduce shadow acne)
    float bias = max(0.0025 * (1.0 - dot(N, Ld)), 0.0008);

    // PCF 3x3 (soft edges)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow; // 0 = lit, 1 = full shadow
}

void main()
{
    vec3 N = normalize(fragNormalEye);
    vec3 V = normalize(-fragPosEye);

    vec3 albedo = (hasDiffuseTex == 1) ? texture(diffuseTexture, fragTexCoords).rgb : baseColor;

    // Directional
    vec3 Ld = normalize(lightDir);

    float ambientStrength = 0.30;
    vec3 ambient = ambientStrength * lightColor;

    float diffD = max(dot(N, Ld), 0.0);
    vec3 diffuseD = diffD * lightColor;

    float specularStrength = 0.08;
    vec3 Rd = reflect(-Ld, N);
    float specD = pow(max(dot(V, Rd), 0.0), 64.0);
    vec3 specularD = specularStrength * specD * lightColor;

    // SHADOW apply only on directional diffuse/spec (not on ambient)
    float shadow = 0.0;
    if (enableShadows == 1) {
        shadow = ShadowFactor(fragPosLightSpace, N, Ld);
    }

    vec3 color = ambient * albedo
               + (1.0 - shadow) * (diffuseD * albedo + specularD);

    // Point lights (unshadowed for simplicity)
    // IMPORTANT: atenuare mai "blanda" ca sa se vada in oras + ceata
    float constant = 1.0;
    float linear = 0.02;
    float quadratic = 0.001;

    // boost ca sa fie vizibil (mai ales cu fog)
    float pointIntensity = 6.0;

    for (int i = 0; i < numPointLights; i++)
    {
        vec3 Lvec = pointLightPosEye[i] - fragPosEye;
        float dist = length(Lvec);
        vec3 Lp = Lvec / max(dist, 1e-6);

        float att = 1.0 / (constant + linear * dist + quadratic * dist * dist);

        float diffP = max(dot(N, Lp), 0.0);
        vec3 diffuseP = diffP * pointLightColor[i] * pointIntensity;

        vec3 Rp = reflect(-Lp, N);
        float specP = pow(max(dot(V, Rp), 0.0), 64.0);
        vec3 specularP = 0.06 * specP * pointLightColor[i] * pointIntensity;

        color += att * ((diffuseP * albedo) + specularP);
    }

    color = clamp(color, 0.0, 1.0);

    // FOG (toggle)
    vec3 finalColor = color;

    if (fogEnabled == 1) {
        float d = length(fragPosEye);
        float fogFactor = clamp((fogEnd - d) / (fogEnd - fogStart), 0.0, 1.0);
        finalColor = mix(fogColor, color, fogFactor);
    }

    fColor = vec4(finalColor, 1.0);

}
