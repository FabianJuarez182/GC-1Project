#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
struct Camera {
    glm::vec3 cameraPosition;
    glm::vec3 targetPosition;
    glm::vec3 upVector;

    // Constructor
    Camera(const glm::vec3& initialPosition, const glm::vec3& initialTarget, const glm::vec3& initialUp)
        : cameraPosition(initialPosition), targetPosition(initialTarget), upVector(initialUp) {}

    // Función para mover la cámara hacia adelante
    void moveForward(float speed) {
        glm::vec3 cameraDirection = glm::normalize(targetPosition - cameraPosition);
        cameraPosition += cameraDirection * speed;
        targetPosition = cameraPosition + cameraDirection;
    }

    // Función para mover la cámara hacia atrás
    void moveBackward(float speed) {
        glm::vec3 cameraDirection = glm::normalize(targetPosition - cameraPosition);
        cameraPosition -= cameraDirection * speed;
        targetPosition = cameraPosition + cameraDirection;
    }

    // Función para rotar la cámara hacia la izquierda
    void rotateLeft(float angle) {
        glm::vec3 cameraDirection = glm::normalize(targetPosition - cameraPosition);
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(angle), upVector);
        cameraDirection = glm::vec3(rotation * glm::vec4(cameraDirection, 0.0f)); // Cambiar 1.0f a 0.0f
        targetPosition = cameraPosition + cameraDirection;
    }

    // Función para rotar la cámara hacia la derecha
    void rotateRight(float angle) {
        glm::vec3 cameraDirection = glm::normalize(targetPosition - cameraPosition);
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-angle), upVector); // Usar un ángulo negativo aquí
        cameraDirection = glm::vec3(rotation * glm::vec4(cameraDirection, 0.0f)); // Cambiar 1.0f a 0.0f
        targetPosition = cameraPosition + cameraDirection;
    }

    // Función para actualizar la vista (deberías llamar a esta función después de cambiar la posición o el objetivo de la cámara)
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(cameraPosition, targetPosition, upVector);
    }
};
