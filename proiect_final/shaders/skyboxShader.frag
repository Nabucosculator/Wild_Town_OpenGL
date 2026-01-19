#version 410 core

in vec3 textureCoordinates;
out vec4 color;

uniform samplerCube skybox;

// optional: tinem skybox-ul “inghitit” de ceata
uniform vec3 fogColor;
uniform float skyboxFog; // 0..1 (ex: 0.6)

void main()
{
    vec3 sky = texture(skybox, textureCoordinates).rgb;
    vec3 finalColor = mix(sky, fogColor, clamp(skyboxFog, 0.0, 1.0));
    color = vec4(finalColor, 1.0);
}
