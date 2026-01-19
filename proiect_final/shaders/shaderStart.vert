#version 410 core

layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 baseColor;

out vec3 color;
out vec3 FragPosWorld; // +++ pentru ceata (world-space)

void main()
{
    // world position for fog distance
    vec4 worldPos = model * vec4(vPosition, 1.0);
    FragPosWorld = worldPos.xyz;

    // compute eye coords
    vec4 vertPosEye = view * worldPos;
    vec3 normalEye = normalize(normalMatrix * vNormal);

    vec3 lightDirN = normalize(lightDir);
    vec3 viewDir = normalize(-vertPosEye.xyz);

    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    vec3 diffuse = max(dot(normalEye, lightDirN), 0.0) * lightColor;

    float specularStrength = 0.5;
    vec3 reflectDir = reflect(-lightDirN, normalEye);
    float specCoeff = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * specCoeff * lightColor;

    color = min((ambient + diffuse) * baseColor + specular, vec3(1.0));

    gl_Position = projection * view * worldPos;
}
