#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <vector>
#include <cassert>
#include "color.h"
#include "print.h"
#include "framebuffer.h"
#include "uniforms.h"
#include "shaders.h"
#include "fragment.h"
#include "texture.h"
#include "vertex.h"
#include "triangle.h"
#include "camera.h"
#include "ObjLoader.h"
#include "noise.h"


SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
Color currentColor;
FragmentShaderType currentFragmentShaderType = FragmentShaderType::Sun; // Inicializa con el tipo de fragment shader deseado

bool moonPresent = false;

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "Error: SDL_Init failed." << std::endl;
        return false;
    }

    window = SDL_CreateWindow("SDL_SpaceShip", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << "Error: Could not create SDL window." << std::endl;
        SDL_Quit();
        return false;
    }


    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "Error: Could not create SDL renderer." << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    setupNoise();

    return true;
}

void setColor(const Color& color) {
    currentColor = color;
}

// Step 1: Vertex Shader
std::vector<Vertex> vertexShaderStep(const std::vector<glm::vec3>& VBO, const Uniforms& uniforms) {
    std::vector<Vertex> transformedVertices(VBO.size() / 3);
    for (size_t i = 0; i < VBO.size() / 3; ++i) {
        Vertex vertex = { VBO[i * 3], VBO[i * 3 + 1], VBO[i * 3 + 2] };;
        transformedVertices[i] = vertexShader(vertex, uniforms);
    }
    return transformedVertices;
}

// Step 2: Primitive Assembly
std::vector<std::vector<Vertex>> primitiveAssemblyStep(const std::vector<Vertex>& transformedVertices) {
    std::vector<std::vector<Vertex>> assembledVertices(transformedVertices.size() / 3);
    for (size_t i = 0; i < transformedVertices.size() / 3; ++i) {
        Vertex edge1 = transformedVertices[3 * i];
        Vertex edge2 = transformedVertices[3 * i + 1];
        Vertex edge3 = transformedVertices[3 * i + 2];
        assembledVertices[i] = { edge1, edge2, edge3 };
    }
    return assembledVertices;
}

// Step 3: Rasterization
std::vector<Fragment> rasterizationStep(const std::vector<std::vector<Vertex>>& assembledVertices) {
    std::vector<Fragment> concurrentFragments;
    for (size_t i = 0; i < assembledVertices.size(); ++i) {
        std::vector<Fragment> rasterizedTriangle = triangle(
            assembledVertices[i][0],
            assembledVertices[i][1],
            assembledVertices[i][2]
        );
        std::lock_guard<std::mutex> lock(std::mutex);
        for (const auto& fragment : rasterizedTriangle) {
            concurrentFragments.push_back(fragment);
        }
    }
    return concurrentFragments;
}

// Step 4: Fragment Shader
void fragmentShaderStep( std::vector<Fragment>& concurrentFragments, FragmentShaderType shaderType) {
for (size_t i = 0; i < concurrentFragments.size(); ++i) {
        const Fragment& fragment = fragmentShader(concurrentFragments[i], shaderType);
        point(fragment); // Be aware of potential race conditions here
    }
}

// Combined function
void render(const std::vector<glm::vec3>& VBO, const Uniforms& uniforms) {
    // Step 1: Vertex Shader
    std::vector<Vertex> transformedVertices = vertexShaderStep(VBO, uniforms);

    // Step 2: Primitive Assembly
    std::vector<std::vector<Vertex>> assembledVertices = primitiveAssemblyStep(transformedVertices);

    // Step 3: Rasterization
    std::vector<Fragment> concurrentFragments = rasterizationStep(assembledVertices);

    // Step 4: Fragment Shader
    fragmentShaderStep(concurrentFragments, currentFragmentShaderType);

    if (moonPresent) {
            fragmentShaderStep(concurrentFragments, currentFragmentShaderType); // Usa el shader de la Luna
    }
}

void toggleFragmentShader() {
    switch (currentFragmentShaderType) {
        case FragmentShaderType::Stripes:
            currentFragmentShaderType = FragmentShaderType::Urano;
            break;
        case FragmentShaderType::Urano:
            currentFragmentShaderType = FragmentShaderType::Mars;
            break;
        case FragmentShaderType::Mars:
            currentFragmentShaderType = FragmentShaderType::Earth;
            break;
        case FragmentShaderType::Earth:
            currentFragmentShaderType = FragmentShaderType::Heat;
            break;
        case FragmentShaderType::Heat:
            currentFragmentShaderType = FragmentShaderType::Sun;
            break;
        case FragmentShaderType::Sun:
            currentFragmentShaderType = FragmentShaderType::Stripes;
            break;
    }
}


glm::mat4 createViewportMatrix(size_t screenWidth, size_t screenHeight) {
    glm::mat4 viewport = glm::mat4(1.0f);

    // Scale
    viewport = glm::scale(viewport, glm::vec3(screenWidth / 2.0f, screenHeight / 2.0f, 0.5f));

    // Translate
    viewport = glm::translate(viewport, glm::vec3(1.0f, 1.0f, 0.5f));

    return viewport;
}

int main(int argc, char* argv[]) {
    if (!init()) {
        return 1;
    }

    float ox = SDL_GetTicks() * 0.1f;
    float oy = SDL_GetTicks() * 0.1f;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
        std::vector<glm::vec3> texCoords;
    std::vector<Face> faces;
    std::vector<glm::vec3> vertexBufferObject; // This will contain both vertices and normals

    std::string filePath = "src/sphere.obj";

    if (!loadOBJ(filePath, vertices, normals, texCoords,  faces)) {
            std::cout << "Error: Could not load OBJ file." << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::string spaceshipFilePath = "src/sphere.obj";
    std::vector<glm::vec3> spaceshipVertices;
    std::vector<glm::vec3> spaceshipNormals;
    std::vector<glm::vec3> spaceshipTexCoords;
    std::vector<Face> spaceshipFaces;
    std::vector<glm::vec3> vertexBufferSpace;

    if (!loadOBJ(spaceshipFilePath, spaceshipVertices, spaceshipNormals, spaceshipTexCoords, spaceshipFaces)) {
        std::cout << "Error: Could not load spaceship OBJ file." << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    for (const auto& face : faces)
    {
        for (int i = 0; i < 3; ++i)
        {
            // Get the vertex position
            glm::vec3 vertexPosition = vertices[face.vertexIndices[i]];

            // Get the normal for the current vertex
            glm::vec3 vertexNormal = normals[face.normalIndices[i]];

            // Get the texture for the current vertex
            glm::vec3 vertexTexture = texCoords[face.texIndices[i]];

            // Add the vertex position and normal to the vertex array
            vertexBufferObject.push_back(vertexPosition);
            vertexBufferObject.push_back(vertexNormal);
            vertexBufferObject.push_back(vertexTexture);
        }
    }

    for (const auto& faceSpace : spaceshipFaces)
    {
        for (int i = 0; i < 3; ++i)
        {
            // Get the vertex position
            glm::vec3 vertexPositionSpace = vertices[faceSpace.vertexIndices[i]];

            // Get the normal for the current vertex
            glm::vec3 vertexNormalSpace = normals[faceSpace.normalIndices[i]];

            // Get the texture for the current vertex
            glm::vec3 vertexTextureSpace = texCoords[faceSpace.texIndices[i]];

            // Add the vertex position and normal to the vertex array
            vertexBufferSpace.push_back(vertexPositionSpace);
            vertexBufferSpace.push_back(vertexNormalSpace);
            vertexBufferSpace.push_back(vertexTextureSpace);
        }
    }

    Uniforms uniforms;

    glm::mat4 model = glm::mat4(1);
    glm::mat4 view = glm::mat4(1);
    glm::mat4 projection = glm::mat4(1);

    glm::vec3 translationVector(0.0f, 0.0f, 0.0f);
    float a = 45.0f;
    glm::vec3 rotationAxis(0.0f, 1.0f, 0.0f); // Rotate around the Y-axis
    glm::vec3 scaleFactor(1.0f, 1.0f, 1.0f);

    glm::mat4 translation = glm::translate(glm::mat4(1.0f), translationVector);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), scaleFactor);

    // Initialize a Camera object
    Camera camera(glm::vec3(0.0f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Projection matrix
    float fovRadians = glm::radians(45.0f);
    float aspectRatio = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT); // Assuming a screen resolution of 800x600
    float nearClip = 0.1f;
    float farClip = 100.0f;
    uniforms.projection = glm::perspective(fovRadians, aspectRatio, nearClip, farClip);

    uniforms.viewport = createViewportMatrix(SCREEN_WIDTH, SCREEN_HEIGHT);
    Uint32 frameStart, frameTime;
    std::string title = "FPS: ";
    int speed = 10;

    bool running = true;
    while (running) {
        frameStart = SDL_GetTicks();

        // Calculate frames per second and update window title
        if (frameTime > 0) {
            std::ostringstream titleStream;
            titleStream << "FPS: " << 1000.0 / frameTime;  // Milliseconds to seconds
            SDL_SetWindowTitle(window, titleStream.str().c_str()); // Use the global 'window' pointer
        }
        glm::vec3 spaceshipPosition;
        float distanceAhead = 1.0f;
        glm::mat4 SpaceModel = glm::mat4(1.0f);
        glm::mat4 rotate;
            SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:
                    camera.rotateLeft(5.0f); // Mueve la cámara hacia la izquierda
                    oy = oy + -0.5;

                    // Aplica una rotación a la nave espacial cuando se presiona la flecha izquierda
                    rotate = glm::rotate(glm::mat4(1.0f), glm::radians(5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    SpaceModel = rotate * SpaceModel;
                    break;
                case SDLK_RIGHT:
                    camera.rotateRight(5.0f); // Mueve la cámara hacia la derecha
                    oy = oy + 0.5;

                    // Aplica una rotación a la nave espacial cuando se presiona la flecha derecha
                    rotate = glm::rotate(glm::mat4(1.0f), glm::radians(-5.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    SpaceModel = rotate * SpaceModel;
                    break;
                case SDLK_UP:
                    camera.moveForward(0.1f); // Mueve la cámara hacia adelante
                    //ox = ox + 1;
                    break;
                case SDLK_DOWN:
                    camera.moveBackward(0.1f); // Mueve la cámara hacia atrás
                    //ox = ox - 1;
                    break;
                }
            }
        }

        a += 0.8;
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(a), rotationAxis);

        // Calculate the model matrix
        uniforms.model = translation * rotation * scale;

        // Create the view matrix using the Camera object
        uniforms.view = glm::lookAt(
            camera.cameraPosition, // The position of the camera
            camera.targetPosition, // The point the camera is looking at
            camera.upVector        // The up vector defining the camera's orientation
        );

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        clearFramebuffer();
        paintSpaceFramebuffer(ox, oy);

        // Render the Sun (center)
        glm::mat4 sunModel = glm::mat4(1.0f);
        sunModel = glm::scale(sunModel, glm::vec3(3.0f));

        Uniforms sunUniforms = uniforms;
        sunUniforms.model = sunModel;
        currentFragmentShaderType = FragmentShaderType::Sun;
        render(vertexBufferObject, sunUniforms);

        // Render the Random Planet
        float heatOrbitRadius = 4.0f; // Adjust the radius as needed
        float heatRotationAngle = glm::radians(a); // Adjust rotation speed as needed

        float heatXPosition = heatOrbitRadius * glm::cos(heatRotationAngle);
        float heatZPosition = heatOrbitRadius * glm::sin(heatRotationAngle);

        glm::mat4 heatModel = glm::mat4(1.0f);
        heatModel = glm::translate(heatModel, glm::vec3(heatXPosition, 0.0f, heatZPosition));
        heatModel = glm::rotate(heatModel, heatRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        heatModel = glm::scale(heatModel, glm::vec3(1.0f)); // Adjust scale as needed

        Uniforms heatUniforms = uniforms;
        heatUniforms.model = heatModel;
        currentFragmentShaderType = FragmentShaderType::Heat;
        render(vertexBufferObject, heatUniforms);

        glm::vec3 front = glm::normalize(camera.targetPosition - camera.cameraPosition);

        // Calcula la nueva posición de la nave espacial (un poco delante de la cámara)
        spaceshipPosition = camera.cameraPosition + front * distanceAhead;
        spaceshipPosition.y = spaceshipPosition.y + -0.35f;

        // Calcula la matriz de modelo para la nave espacial
        float r = 0.0f;
        SpaceModel = glm::translate(SpaceModel, spaceshipPosition);
        //float spaceshipRotationAngle = glm::radians(r);
        //SpaceModel = glm::rotate(glm::mat4(1.0f), spaceshipRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        SpaceModel = glm::scale(SpaceModel, glm::vec3(0.25f));

        // Renderiza la nave espacial
        Uniforms spaceshipUniforms = uniforms;
        spaceshipUniforms.model = SpaceModel;
        currentFragmentShaderType = FragmentShaderType::Kirby;
        render(vertexBufferSpace, spaceshipUniforms);

        // Render the Marte
        float marsOrbitRadius = 6.0f;
        float marsRotationAngle = glm::radians(a); // Adjust rotation speed as needed

        float marsXPosition = marsOrbitRadius * glm::cos(marsRotationAngle);
        float marsZPosition = marsOrbitRadius * glm::sin(marsRotationAngle);

        glm::mat4 marsModel = glm::mat4(1.0f);
        marsModel = glm::translate(marsModel, glm::vec3(marsXPosition, 0.0f, marsZPosition));
        marsModel = glm::rotate(marsModel, marsRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        marsModel = glm::scale(marsModel, glm::vec3(0.8f)); // Adjust scale as needed

        Uniforms marsUniforms = uniforms;
        marsUniforms.model = marsModel;
        currentFragmentShaderType = FragmentShaderType::Mars;
        render(vertexBufferObject, marsUniforms);

        // Render the Jupiter
        float stripesOrbitRadius = 8.0f; // Adjust the radius as needed
        float stripesRotationAngle = glm::radians(a); // Adjust rotation speed as needed

        float stripesXPosition = stripesOrbitRadius * glm::cos(stripesRotationAngle);
        float stripesZPosition = stripesOrbitRadius * glm::sin(stripesRotationAngle);

        glm::mat4 stripesModel = glm::mat4(1.0f);
        stripesModel = glm::translate(stripesModel, glm::vec3(stripesXPosition, 0.0f, stripesZPosition));
        stripesModel = glm::rotate(stripesModel, stripesRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        stripesModel = glm::scale(stripesModel, glm::vec3(1.8f)); // Adjust scale as needed

        Uniforms stripesUniforms = uniforms;
        stripesUniforms.model = stripesModel;
        currentFragmentShaderType = FragmentShaderType::Stripes;
        render(vertexBufferObject, stripesUniforms);


        // Render the Earth
        float earthOrbitRadius = 11.0f; // Adjust the radius as needed
        float earthRotationAngle = glm::radians(a); // Adjust rotation speed as needed

        float earthXPosition = earthOrbitRadius * glm::cos(earthRotationAngle);
        float earthZPosition = earthOrbitRadius * glm::sin(earthRotationAngle);

        glm::mat4 earthModel = glm::mat4(1.0f);
        earthModel = glm::translate(earthModel, glm::vec3(earthXPosition, 0.0f, earthZPosition));
        earthModel = glm::rotate(earthModel, earthRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        earthModel = glm::scale(earthModel, glm::vec3(1.0f)); // Adjust scale as needed

        Uniforms earthUniforms = uniforms;
        earthUniforms.model = earthModel;
        currentFragmentShaderType = FragmentShaderType::Earth;
        render(vertexBufferObject, earthUniforms);

        if (currentFragmentShaderType == FragmentShaderType::Earth) {
            moonPresent = true;
            // Define las coordenadas de traslación de la Luna
            float x_translation = 3.0f;
            float y_translation = 0.0f;
            float z_translation = 0.0f;
            float earthRotationAngle = glm::radians(a);

                // Radio de la órbita de la Luna alrededor de la Tierra
            float moonOrbitRadius = 1.0f; // Ajusta el radio según tus necesidades

            // Ángulo de rotación de la Luna alrededor de la Tierra (puedes ajustarlo)
            float moonRotationAngle = glm::radians(a * -1.5f); // Puedes ajustar el factor de rotación según tus necesidades

            // Posición de la Luna en relación con la Tierra
            float moonXPosition = moonOrbitRadius * glm::cos(moonRotationAngle);
            float moonZPosition = moonOrbitRadius * glm::sin(moonRotationAngle);

            // Matriz de modelo para la Luna
            glm::mat4 moonModel = glm::mat4(1.0f);
            moonModel = glm::translate(earthModel, glm::vec3(moonXPosition, 0.0f, moonZPosition));

            glm::vec3 earthRotationAxis(0.0f, 1.0f, 0.0f); // Eje de rotación de la Tierra (en este caso, eje Y)
            moonModel = glm::rotate(moonModel, earthRotationAngle, earthRotationAxis);

            float moonScaleFactor = 0.4f;
            moonModel = glm::scale(moonModel, glm::vec3(moonScaleFactor, moonScaleFactor, moonScaleFactor));

            // Calcula la matriz de vista y proyección para la Luna (usa las mismas que para la Tierra)
            Uniforms moonUniforms = uniforms;
            moonUniforms.model = moonModel;

            // Guarda el tipo de fragment shader actual de la Tierra
            FragmentShaderType originalEarthFragmentShaderType = currentFragmentShaderType;

            // Establece el tipo de fragment shader específico para la Luna
            currentFragmentShaderType = FragmentShaderType::Moon;

            // Renderiza la Luna
            render(vertexBufferObject, moonUniforms);

            // Restaura el tipo de fragment shader de la Tierra
            currentFragmentShaderType = originalEarthFragmentShaderType;
        }

        // Render the Urano
        float uranoOrbitRadius = 13.0f; // Adjust the radius as needed
        float uranoRotationAngle = glm::radians(a); // Adjust rotation speed as needed

        float uranoXPosition = uranoOrbitRadius * glm::cos(uranoRotationAngle);
        float uranoZPosition = uranoOrbitRadius * glm::sin(uranoRotationAngle);

        glm::mat4 uranoModel = glm::mat4(1.0f);
        uranoModel = glm::translate(uranoModel, glm::vec3(uranoXPosition, 0.0f, uranoZPosition));
        uranoModel = glm::rotate(uranoModel, uranoRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        uranoModel = glm::scale(uranoModel, glm::vec3(0.5f)); // Adjust scale as needed

        Uniforms uranoUniforms = uniforms;
        uranoUniforms.model = uranoModel;
        currentFragmentShaderType = FragmentShaderType::Urano;
        render(vertexBufferObject, uranoUniforms);


        renderBuffer(renderer);

        frameTime = SDL_GetTicks() - frameStart;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
