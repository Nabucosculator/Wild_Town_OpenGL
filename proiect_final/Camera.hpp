#ifndef Camera_hpp
#define Camera_hpp

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace gps {

    enum MOVE_DIRECTION { MOVE_FORWARD, MOVE_BACKWARD, MOVE_RIGHT, MOVE_LEFT };

    class Camera {
    public:
        Camera(glm::vec3 cameraPosition, glm::vec3 cameraTarget, glm::vec3 cameraUp);

        glm::mat4 getViewMatrix();

        void move(MOVE_DIRECTION direction, float speed);
        void rotate(float pitchDelta, float yawDelta);

        // NEW (needed for collisions / ground clamp / free roam)
        glm::vec3 getPosition() const { return cameraPosition; }
        void setPosition(const glm::vec3& p) {
            cameraPosition = p;
            cameraTarget = cameraPosition + cameraFrontDirection;
        }

        glm::vec3 getFront() const { return cameraFrontDirection; }
        glm::vec3 getRight() const { return cameraRightDirection; }

    private:
        glm::vec3 cameraPosition;
        glm::vec3 cameraTarget;

        glm::vec3 cameraFrontDirection;
        glm::vec3 cameraRightDirection;
        glm::vec3 cameraUpDirection;

        float yawDeg;
        float pitchDeg;

        void updateCameraVectors();
    };
}

#endif /* Camera_hpp */
