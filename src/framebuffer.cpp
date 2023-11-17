#include "framebuffer.h"
#include "FastNoiseLite.h"

FragColor blank{
    Color{0, 0, 0},
    std::numeric_limits<double>::max()
};

FragColor star{
    Color{255, 255, 255},
    std::numeric_limits<double>::max()
};

std::array<FragColor, SCREEN_WIDTH * SCREEN_HEIGHT> framebuffer;

// Create a 2D array of mutexes
std::array<std::mutex, SCREEN_WIDTH * SCREEN_HEIGHT> mutexes;

void point(Fragment f) {
    std::lock_guard<std::mutex> lock(mutexes[f.y * SCREEN_WIDTH + f.x]);

    if (f.z < framebuffer[f.y * SCREEN_WIDTH + f.x].z) {
        framebuffer[f.y * SCREEN_WIDTH + f.x] = FragColor{f.color, f.z};
    }
}

void clearFramebuffer() {
    std::fill(framebuffer.begin(), framebuffer.end(), blank);
}

void renderBuffer(SDL_Renderer* renderer) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    void* texturePixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &texturePixels, &pitch);

    Uint32 format = SDL_PIXELFORMAT_ARGB8888;
    SDL_PixelFormat* mappingFormat = SDL_AllocFormat(format);

    Uint32* texturePixels32 = static_cast<Uint32*>(texturePixels);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int framebufferY = SCREEN_HEIGHT - y - 1; // Reverse the order of rows
            int index = y * (pitch / sizeof(Uint32)) + x;
            const Color& color = framebuffer[framebufferY * SCREEN_WIDTH + x].color;
            texturePixels32[index] = SDL_MapRGBA(mappingFormat, color.r, color.g, color.b, color.a);
        }
    }

    SDL_UnlockTexture(texture);
    SDL_Rect textureRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderCopy(renderer, texture, NULL, &textureRect);
    SDL_DestroyTexture(texture);

    SDL_RenderPresent(renderer);
}

void paintSpaceFramebuffer(float ox, float oy) {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            FastNoiseLite noiseGenerator;
            noiseGenerator.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

            float scale = 1000.0f;
            float noiseValue = noiseGenerator.GetNoise((x + (ox * 100.0f)) * scale, (y + oy * 100.0f) * scale);

            // Si el valor de ruido está por encima de un umbral, dibujar una estrella
            if (noiseValue > 0.97f) {
                // Vary the size and intensity of stars
                float starSize = noiseValue * 2.0f; // Tamaño de estrella individual

                // Escala el tamaño de la estrella individual
                starSize = std::max(1.0f, starSize); // Asegurarse de que el tamaño mínimo sea 1.0f

                // Escala la intensidad de la estrella
                float starIntensity = (noiseValue + 1.0f) / 2.0f; // Intensidad de estrella
                starIntensity = std::min(1.0f, starIntensity * 1.5f);

                // Establece el color de la estrella basado en la intensidad
                Color starColor = {
                    static_cast<uint8_t>(starIntensity * 255),
                    static_cast<uint8_t>(starIntensity * 255),
                    static_cast<uint8_t>(starIntensity * 255),
                    255
                };

                // Crea un FragColor con el color y la intensidad de la estrella
                FragColor starFragColor;
                starFragColor.color = starColor;
                starFragColor.z = starIntensity;

                // Dibuja la estrella en el framebuffer con el tamaño escalado
                int startX = x - static_cast<int>(starSize / 2);
                int startY = y - static_cast<int>(starSize / 2);
                int endX = x + static_cast<int>(starSize / 2);
                int endY = y + static_cast<int>(starSize / 2);

                for (int starX = startX; starX <= endX; starX++) {
                    for (int starY = startY; starY <= endY; starY++) {
                        if (starX >= 0 && starX < SCREEN_WIDTH && starY >= 0 && starY < SCREEN_HEIGHT) {
                            framebuffer[starY * SCREEN_WIDTH + starX] = starFragColor;
                        }
                    }
                }
            } else {
                framebuffer[y * SCREEN_WIDTH + x] = blank;
            }
        }
    }
}
