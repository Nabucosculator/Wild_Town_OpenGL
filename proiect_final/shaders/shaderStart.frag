#version 410 core

in vec3 color;
in vec3 FragPosWorld;

out vec4 fColor;

// Silent Hill fog
uniform int fogEnabled; // 0/1
uniform vec3 cameraPos; // world-space camera position
uniform vec3 fogColor;  // ex: vec3(0.68, 0.65, 0.72)
uniform float fogStart; // ex: 5.0
uniform float fogEnd;   // ex: 25.0

void main()
{
    vec3 finalColor = color;

    if (fogEnabled == 1) {
        float distToCam = distance(cameraPos, FragPosWorld);

        float fogFactor = (fogEnd - distToCam) / (fogEnd - fogStart);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        finalColor = mix(fogColor, color, fogFactor);
    }

    fColor = vec4(finalColor, 1.0);
}

