#version 410 core

layout (location = 0) in vec3 vertexPosition;

out vec3 textureCoordinates;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    // view vine deja fara translatie din SkyBox::Draw (mat4(mat3(view)))
    vec4 pos = projection * view * vec4(vertexPosition, 1.0);

    // trucul de skybox: fortam depth = 1.0
    gl_Position = pos.xyww;

    textureCoordinates = vertexPosition;
}
