#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace gps {

    Camera::Camera(glm::vec3 cameraPosition, glm::vec3 cameraTarget, glm::vec3 cameraUp)
    {
        this->cameraPosition = cameraPosition;
        this->cameraTarget = cameraTarget;

        this->cameraUpDirection = glm::normalize(cameraUp);

        this->cameraFrontDirection = glm::normalize(cameraTarget - cameraPosition);

        this->yawDeg = glm::degrees(atan2(this->cameraFrontDirection.z, this->cameraFrontDirection.x));
        this->pitchDeg = glm::degrees(asin(this->cameraFrontDirection.y));

        updateCameraVectors();
        this->cameraTarget = this->cameraPosition + this->cameraFrontDirection;
    }

    glm::mat4 Camera::getViewMatrix()
    {
        return glm::lookAt(cameraPosition, cameraTarget, cameraUpDirection);
    }

    void Camera::move(MOVE_DIRECTION direction, float speed)
    {
        glm::vec3 forward = glm::normalize(cameraFrontDirection);
        glm::vec3 right = glm::normalize(cameraRightDirection);

        if (direction == MOVE_FORWARD) cameraPosition += forward * speed;
        else if (direction == MOVE_BACKWARD) cameraPosition -= forward * speed;
        else if (direction == MOVE_RIGHT) cameraPosition += right * speed;
        else if (direction == MOVE_LEFT) cameraPosition -= right * speed;

        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    void Camera::rotate(float pitchDelta, float yawDelta)
    {
        pitchDeg += pitchDelta;
        yawDeg += yawDelta;

        if (pitchDeg > 89.0f) pitchDeg = 89.0f;
        if (pitchDeg < -89.0f) pitchDeg = -89.0f;

        updateCameraVectors();
        cameraTarget = cameraPosition + cameraFrontDirection;
    }

    void Camera::updateCameraVectors()
    {
        float yawRad = glm::radians(yawDeg);
        float pitchRad = glm::radians(pitchDeg);

        glm::vec3 front;
        front.x = cos(yawRad) * cos(pitchRad);
        front.y = sin(pitchRad);
        front.z = sin(yawRad) * cos(pitchRad);

        cameraFrontDirection = glm::normalize(front);

        cameraRightDirection = glm::normalize(glm::cross(cameraFrontDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
        cameraUpDirection = glm::normalize(glm::cross(cameraRightDirection, cameraFrontDirection));
    }
}
