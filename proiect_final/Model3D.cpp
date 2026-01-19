#include "Model3D.hpp"
#include <unordered_map>
#include <cfloat>
#include <algorithm>
#include <cmath>

namespace gps {

    static std::string normalizeTexName(const std::string& texName)
    {
        if (texName.empty()) return texName;
        if (texName[0] == '/' || texName[0] == '\\') return texName.substr(1);
        return texName;
    }

    static bool isTerrainMaterialName(const std::string& name)
    {
        return name.find("Terrain") != std::string::npos || name.find("terrain") != std::string::npos;
    }

    static glm::vec3 vmin3(const glm::vec3& a, const glm::vec3& b) {
        return glm::vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
    }
    static glm::vec3 vmax3(const glm::vec3& a, const glm::vec3& b) {
        return glm::vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
    }

    static void transformAABBToWorld(const glm::mat4& M, const glm::vec3& bmin, const glm::vec3& bmax,
        glm::vec3& outMin, glm::vec3& outMax)
    {
        glm::vec3 corners[8] = {
            {bmin.x, bmin.y, bmin.z},
            {bmax.x, bmin.y, bmin.z},
            {bmin.x, bmax.y, bmin.z},
            {bmax.x, bmax.y, bmin.z},
            {bmin.x, bmin.y, bmax.z},
            {bmax.x, bmin.y, bmax.z},
            {bmin.x, bmax.y, bmax.z},
            {bmax.x, bmax.y, bmax.z},
        };

        outMin = glm::vec3(FLT_MAX);
        outMax = glm::vec3(-FLT_MAX);

        for (int i = 0; i < 8; i++) {
            glm::vec4 w = M * glm::vec4(corners[i], 1.0f);
            glm::vec3 p(w.x, w.y, w.z);
            outMin = vmin3(outMin, p);
            outMax = vmax3(outMax, p);
        }
    }

    bool Model3D::rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
        float& tHit)
    {
        const float EPS = 1e-7f;
        glm::vec3 e1 = v1 - v0;
        glm::vec3 e2 = v2 - v0;

        glm::vec3 pvec = glm::cross(dir, e2);
        float det = glm::dot(e1, pvec);

        if (fabs(det) < EPS) return false;
        float invDet = 1.0f / det;

        glm::vec3 tvec = orig - v0;
        float u = glm::dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        glm::vec3 qvec = glm::cross(tvec, e1);
        float v = glm::dot(dir, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = glm::dot(e2, qvec) * invDet;
        if (t < 0.0f) return false;

        tHit = t;
        return true;
    }

    void Model3D::LoadModel(std::string fileName)
    {
        std::string basePath = fileName.substr(0, fileName.find_last_of('/')) + "/";
        ReadOBJ(fileName, basePath);
    }

    void Model3D::LoadModel(std::string fileName, std::string basePath)
    {
        ReadOBJ(fileName, basePath);
    }

    void Model3D::Draw(gps::Shader shaderProgram)
    {
        for (int i = 0; i < (int)meshes.size(); i++)
            meshes[i].Draw(shaderProgram);
    }

    // --- helper for collision grid keys
    static long long packKey(int cx, int cz)
    {
        // pack two 32-bit signed ints into one 64-bit
        return ((long long)cx << 32) ^ (unsigned int)cz;
    }

    void Model3D::ReadOBJ(std::string fileName, std::string basePath)
    {
        std::cout << "Loading : " << fileName << std::endl;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
            fileName.c_str(), basePath.c_str(), GL_TRUE);

        if (!err.empty()) std::cerr << err << std::endl;
        if (!ret) exit(1);

        std::cout << "# of shapes    : " << shapes.size() << std::endl;
        std::cout << "# of materials : " << materials.size() << std::endl;

        meshes.clear();
        loadedTextures.clear();
        terrainTriangles.clear();
        sceneCollidersLocal.clear();

        // === NEW: collision grid (MODEL-LOCAL)
        // cell size in LOCAL units (model is huge before scaling)
        const float cellSize = 250.0f; // adjust if needed (200..500)
        std::unordered_map<long long, AABB> collisionCells;

        for (size_t s = 0; s < shapes.size(); s++)
        {
            struct SubMesh {
                std::vector<gps::Vertex> vertices;
                std::vector<GLuint> indices;
            };

            std::unordered_map<int, SubMesh> byMat;
            size_t index_offset = 0;

            for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
            {
                int fv = shapes[s].mesh.num_face_vertices[f];

                int matId = -1;
                if (f < shapes[s].mesh.material_ids.size())
                    matId = shapes[s].mesh.material_ids[f];

                SubMesh& sm = byMat[matId];

                // collect local positions for terrain build
                std::vector<glm::vec3> facePosLocal;
                facePosLocal.reserve(fv);

                bool faceIsTerrain = false;
                if (matId >= 0 && matId < (int)materials.size()) {
                    faceIsTerrain = isTerrainMaterialName(materials[matId].name);
                }

                for (int v = 0; v < fv; v++)
                {
                    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                    float vx = attrib.vertices[3 * idx.vertex_index + 0];
                    float vy = attrib.vertices[3 * idx.vertex_index + 1];
                    float vz = attrib.vertices[3 * idx.vertex_index + 2];

                    float nx = 0.0f, ny = 1.0f, nz = 0.0f;
                    if (idx.normal_index != -1 && !attrib.normals.empty()) {
                        nx = attrib.normals[3 * idx.normal_index + 0];
                        ny = attrib.normals[3 * idx.normal_index + 1];
                        nz = attrib.normals[3 * idx.normal_index + 2];
                    }

                    float tx = 0.0f, ty = 0.0f;
                    if (idx.texcoord_index != -1 && !attrib.texcoords.empty()) {
                        tx = attrib.texcoords[2 * idx.texcoord_index + 0];
                        ty = attrib.texcoords[2 * idx.texcoord_index + 1];
                    }

                    gps::Vertex vert;
                    vert.Position = glm::vec3(vx, vy, vz);
                    vert.Normal = glm::vec3(nx, ny, nz);
                    vert.TexCoords = glm::vec2(tx, ty);

                    sm.vertices.push_back(vert);
                    sm.indices.push_back((GLuint)(sm.vertices.size() - 1));

                    facePosLocal.push_back(vert.Position);

                    // === NEW: build colliders in a GRID (skip terrain)
                    if (!faceIsTerrain) {
                        int cx = (int)std::floor(vert.Position.x / cellSize);
                        int cz = (int)std::floor(vert.Position.z / cellSize);
                        long long key = packKey(cx, cz);

                        auto it = collisionCells.find(key);
                        if (it == collisionCells.end()) {
                            AABB box;
                            box.minP = vert.Position;
                            box.maxP = vert.Position;
                            collisionCells.emplace(key, box);
                        }
                        else {
                            it->second.minP = vmin3(it->second.minP, vert.Position);
                            it->second.maxP = vmax3(it->second.maxP, vert.Position);
                        }
                    }
                }

                // terrain triangles
                if (faceIsTerrain)
                {
                    for (int i = 1; i + 1 < (int)facePosLocal.size(); i++) {
                        Triangle t;
                        t.a = facePosLocal[0];
                        t.b = facePosLocal[i];
                        t.c = facePosLocal[i + 1];
                        terrainTriangles.push_back(t);
                    }
                }

                index_offset += fv;
            }

            // build render meshes per material (unchanged for rendering)
            for (auto& kv : byMat)
            {
                int matId = kv.first;
                SubMesh& sm = kv.second;

                std::vector<gps::Texture> textures;
                glm::vec3 kd(1.0f, 1.0f, 1.0f);

                if (matId >= 0 && matId < (int)materials.size())
                {
                    const auto& m = materials[matId];
                    kd = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);

                    std::string diffuseTex = normalizeTexName(m.diffuse_texname);
                    if (!diffuseTex.empty()) {
                        gps::Texture t = LoadTexture(basePath + diffuseTex, "diffuseTexture");
                        textures.push_back(t);
                    }

                    std::string specTex = normalizeTexName(m.specular_texname);
                    if (!specTex.empty()) {
                        gps::Texture t = LoadTexture(basePath + specTex, "specularTexture");
                        textures.push_back(t);
                    }

                    std::string ambTex = normalizeTexName(m.ambient_texname);
                    if (!ambTex.empty()) {
                        gps::Texture t = LoadTexture(basePath + ambTex, "ambientTexture");
                        textures.push_back(t);
                    }
                }

                meshes.push_back(gps::Mesh(sm.vertices, sm.indices, textures, kd));
            }
        }

        // finalize colliders from grid
        sceneCollidersLocal.reserve(collisionCells.size());
        for (const auto& kv : collisionCells) {
            sceneCollidersLocal.push_back(kv.second);
        }

        std::cout << "Terrain triangles: " << terrainTriangles.size() << std::endl;
        std::cout << "Scene colliders (grid AABB): " << sceneCollidersLocal.size() << std::endl;
    }

    bool Model3D::getGroundHeightAtWorldXZ(const glm::mat4& modelMatrix, float worldX, float worldZ, float& outY) const
    {
        if (terrainTriangles.empty()) return false;

        glm::mat4 invM = glm::inverse(modelMatrix);

        glm::vec4 oWorld(worldX, 100000.0f, worldZ, 1.0f);
        glm::vec4 oLocal4 = invM * oWorld;
        glm::vec3 oLocal(oLocal4.x, oLocal4.y, oLocal4.z);

        glm::vec4 dWorld(0.0f, -1.0f, 0.0f, 0.0f);
        glm::vec4 dLocal4 = invM * dWorld;
        glm::vec3 dLocal = glm::normalize(glm::vec3(dLocal4.x, dLocal4.y, dLocal4.z));

        float bestT = FLT_MAX;
        bool hit = false;

        for (const auto& tri : terrainTriangles)
        {
            float t;
            if (rayTriangleIntersect(oLocal, dLocal, tri.a, tri.b, tri.c, t))
            {
                if (t < bestT) {
                    bestT = t;
                    hit = true;
                }
            }
        }

        if (!hit) return false;

        glm::vec3 pLocal = oLocal + dLocal * bestT;
        glm::vec4 pWorld4 = modelMatrix * glm::vec4(pLocal, 1.0f);
        outY = pWorld4.y;
        return true;
    }

    // push sphere out of colliders
    bool Model3D::resolveSphereCollisions(const glm::mat4& modelMatrix, glm::vec3& inOutWorldPos, float radius) const
    {
        if (sceneCollidersLocal.empty()) return false;

        bool changed = false;

        for (int pass = 0; pass < 3; pass++)
        {
            bool passChanged = false;

            for (const auto& localBox : sceneCollidersLocal)
            {
                glm::vec3 bminW, bmaxW;
                transformAABBToWorld(modelMatrix, localBox.minP, localBox.maxP, bminW, bmaxW);

                // === NEW: Y filter so you DON'T get blocked in air
                if (inOutWorldPos.y > bmaxW.y + radius) continue;
                if (inOutWorldPos.y < bminW.y - radius) continue;

                // expanded box in XZ
                float minX = bminW.x - radius;
                float maxX = bmaxW.x + radius;
                float minZ = bminW.z - radius;
                float maxZ = bmaxW.z + radius;

                bool insideXZ =
                    (inOutWorldPos.x > minX && inOutWorldPos.x < maxX &&
                        inOutWorldPos.z > minZ && inOutWorldPos.z < maxZ);

                if (!insideXZ) continue;

                float penLeft = inOutWorldPos.x - minX;
                float penRight = maxX - inOutWorldPos.x;
                float penBack = inOutWorldPos.z - minZ;
                float penFront = maxZ - inOutWorldPos.z;

                float pushX = (penLeft < penRight) ? -penLeft : penRight;
                float pushZ = (penBack < penFront) ? -penBack : penFront;

                if (fabs(pushX) < fabs(pushZ)) inOutWorldPos.x += pushX;
                else                           inOutWorldPos.z += pushZ;

                passChanged = true;
                changed = true;
            }

            if (!passChanged) break;
        }

        return changed;
    }

    gps::Texture Model3D::LoadTexture(std::string path, std::string type)
    {
        for (int i = 0; i < (int)loadedTextures.size(); i++) {
            if (loadedTextures[i].path == path) {
                return loadedTextures[i];
            }
        }

        gps::Texture currentTexture;
        currentTexture.id = ReadTextureFromFile(path.c_str());
        currentTexture.type = type;
        currentTexture.path = path;

        loadedTextures.push_back(currentTexture);
        return currentTexture;
    }

    GLuint Model3D::ReadTextureFromFile(const char* file_name)
    {
        int x, y, n;
        int force_channels = 4;
        unsigned char* image_data = stbi_load(file_name, &x, &y, &n, force_channels);

        if (!image_data) {
            fprintf(stderr, "ERROR: could not load %s\n", file_name);
            return 0;
        }

        int width_in_bytes = x * 4;
        int half_height = y / 2;
        for (int row = 0; row < half_height; row++) {
            unsigned char* top = image_data + row * width_in_bytes;
            unsigned char* bottom = image_data + (y - row - 1) * width_in_bytes;
            for (int col = 0; col < width_in_bytes; col++) {
                unsigned char temp = top[col];
                top[col] = bottom[col];
                bottom[col] = temp;
            }
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(image_data);

        return textureID;
    }

    Model3D::~Model3D()
    {
        for (size_t i = 0; i < loadedTextures.size(); i++) {
            glDeleteTextures(1, &loadedTextures.at(i).id);
        }

        for (size_t i = 0; i < meshes.size(); i++) {
            GLuint VBO = meshes.at(i).getBuffers().VBO;
            GLuint EBO = meshes.at(i).getBuffers().EBO;
            GLuint VAO = meshes.at(i).getBuffers().VAO;
            glDeleteBuffers(1, &VBO);
            glDeleteBuffers(1, &EBO);
            glDeleteVertexArrays(1, &VAO);
        }
    }
}
