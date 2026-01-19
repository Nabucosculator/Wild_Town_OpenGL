#version 410 core

layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

// NEW
uniform mat4 lightSpaceMatrix;

out vec3 fragPosEye;
out vec3 fragNormalEye;
out vec2 fragTexCoords;

// NEW
out vec4 fragPosLightSpace;

void main()
{
    vec4 posWorld = model * vec4(vPosition, 1.0);
    vec4 posEye = view * posWorld;

    fragPosEye = posEye.xyz;
    fragNormalEye = normalize(normalMatrix * vNormal);
    fragTexCoords = vTexCoords;

    // NEW
    fragPosLightSpace = lightSpaceMatrix * posWorld;

    gl_Position = projection * posEye;
}
