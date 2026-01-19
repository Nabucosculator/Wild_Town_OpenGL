#ifndef Model3D_hpp
#define Model3D_hpp

#include "Mesh.hpp"

#include "tiny_obj_loader.h"
#include "stb_image.h"

#include <iostream>
#include <string>
#include <vector>

namespace gps {

    class Model3D {

    public:
        ~Model3D();

        void LoadModel(std::string fileName);
        void LoadModel(std::string fileName, std::string basePath);
        void Draw(gps::Shader shaderProgram);

        // Uneven terrain support
        bool getGroundHeightAtWorldXZ(const glm::mat4& modelMatrix, float worldX, float worldZ, float& outY) const;

        // Collisions with scene objects (buildings etc.)
        bool resolveSphereCollisions(const glm::mat4& modelMatrix, glm::vec3& inOutWorldPos, float radius) const;

    private:
        std::vector<gps::Mesh> meshes;
        std::vector<gps::Texture> loadedTextures;

        // Terrain triangles stored in MODEL-LOCAL coordinates
        struct Triangle {
            glm::vec3 a;
            glm::vec3 b;
            glm::vec3 c;
        };
        std::vector<Triangle> terrainTriangles;

        // Scene colliders stored in MODEL-LOCAL coordinates
        struct AABB {
            glm::vec3 minP;
            glm::vec3 maxP;
        };
        std::vector<AABB> sceneCollidersLocal;

        void ReadOBJ(std::string fileName, std::string basePath);
        gps::Texture LoadTexture(std::string path, std::string type);
        GLuint ReadTextureFromFile(const char* file_name);

        // Ray-triangle (Moller-Trumbore)
        static bool rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
            const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
            float& tHit);
    };
}

#endif /* Model3D_hpp */
