#include "Mesh.hpp"

namespace gps {

    Mesh::Mesh(std::vector<Vertex> vertices,
        std::vector<GLuint> indices,
        std::vector<Texture> textures,
        glm::vec3 kdColor)
    {
        this->vertices = std::move(vertices);
        this->indices = std::move(indices);
        this->textures = std::move(textures);
        this->kdColor = kdColor;

        this->setupMesh();
    }

    Buffers Mesh::getBuffers() {
        return this->buffers;
    }

    void Mesh::Draw(gps::Shader shader)
    {
        shader.useShaderProgram();

        // ---- uniforms expected by shaderPPL.frag:
        // uniform vec3 baseColor;
        // uniform sampler2D diffuseTexture;
        // uniform int hasDiffuseTex;

        GLint baseColorLoc = glGetUniformLocation(shader.shaderProgram, "baseColor");
        if (baseColorLoc != -1) {
            glUniform3f(baseColorLoc, kdColor.r, kdColor.g, kdColor.b);
        }

        // find diffuse texture (if any)
        GLuint diffuseTexId = 0;
        bool hasDiffuse = false;

        for (const auto& t : textures) {
            if (t.type == "diffuseTexture") {
                diffuseTexId = t.id;
                hasDiffuse = (diffuseTexId != 0);
                break;
            }
        }

        GLint hasDiffuseLoc = glGetUniformLocation(shader.shaderProgram, "hasDiffuseTex");
        if (hasDiffuseLoc != -1) {
            glUniform1i(hasDiffuseLoc, hasDiffuse ? 1 : 0);
        }

        GLint diffuseSamplerLoc = glGetUniformLocation(shader.shaderProgram, "diffuseTexture");
        if (diffuseSamplerLoc != -1) {
            glUniform1i(diffuseSamplerLoc, 0); // we bind diffuse on unit 0
        }

        // bind diffuseTexture only (shader uses only one sampler)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hasDiffuse ? diffuseTexId : 0);

        // draw
        glBindVertexArray(this->buffers.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)this->indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // cleanup
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Mesh::setupMesh()
    {
        glGenVertexArrays(1, &this->buffers.VAO);
        glGenBuffers(1, &this->buffers.VBO);
        glGenBuffers(1, &this->buffers.EBO);

        glBindVertexArray(this->buffers.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, this->buffers.VBO);
        glBufferData(GL_ARRAY_BUFFER,
            this->vertices.size() * sizeof(Vertex),
            this->vertices.data(),
            GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->buffers.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            this->indices.size() * sizeof(GLuint),
            this->indices.data(),
            GL_STATIC_DRAW);

        // layout(location=0) position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);

        // layout(location=1) normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (GLvoid*)offsetof(Vertex, Normal));

        // layout(location=2) texCoords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (GLvoid*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
}
