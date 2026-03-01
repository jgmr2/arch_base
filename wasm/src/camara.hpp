#pragma once
#include <cmath>
#include <algorithm>

struct FrameAnalysis {
    float minX, minY, maxX, maxY;
    float yaw, pitch;
};

namespace CameraUtils {
    // Definimos la función exactamente como la busca main.cpp
    inline FrameAnalysis analyzeAndNormalize(float* lm, int count) {
        float minX = 1.0f, minY = 1.0f, maxX = 0.0f, maxY = 0.0f;
        
        for (int i = 0; i < count; i++) {
            float x = lm[i * 3];
            float y = lm[i * 3 + 1];
            if (x < minX) minX = x; if (x > maxX) maxX = x;
            if (y < minY) minY = y; if (y > maxY) maxY = y;
        }

        float yaw = lm[454 * 3 + 2] - lm[234 * 3 + 2];
        float pitch = lm[10 * 3 + 2] - lm[152 * 3 + 2];

        float anchorX = lm[1 * 3], anchorY = lm[1 * 3 + 1], anchorZ = lm[1 * 3 + 2];
        float h = std::sqrt(std::pow(lm[10 * 3] - lm[152 * 3], 2) + std::pow(lm[10 * 3 + 1] - lm[152 * 3 + 1], 2));
        float w = std::sqrt(std::pow(lm[234 * 3] - lm[454 * 3], 2) + std::pow(lm[234 * 3 + 1] - lm[454 * 3 + 1], 2));
        float scale = (h + w) / 2.0f;

        if (scale > 0.0001f) {
            for (int i = 0; i < count; i++) {
                lm[i * 3]     = (lm[i * 3] - anchorX) / scale;
                lm[i * 3 + 1] = (lm[i * 3 + 1] - anchorY) / scale;
                lm[i * 3 + 2] = (lm[i * 3 + 2] - anchorZ) / (scale * 0.85f);
            }
        }

        return { minX, minY, maxX, maxY, yaw, pitch };
    }
}