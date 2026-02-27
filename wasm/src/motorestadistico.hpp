#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

struct BiometricModel {
    std::vector<float> mean;
    std::vector<float> invCovariance;
    std::string userId;
};

class StatisticalCore {
protected:
    const float RIGOROUS_SMOOTHING = 0.00001f;
    const float ALPHA_DECAY = 0.00015f;
    const float ALPHA_PENALTY_EXPONENT = 1.8f;

    inline float computeMahalanobis(float* vec, float* mean, float* invCov, int dims) {
        float dist = 0.0f;
        for (int i = 0; i < dims; ++i) {
            float diff = vec[i] - mean[i];
            dist += (diff * diff) * invCov[i];
        }
        return std::sqrt(dist);
    }

    inline float computeScore(float dist) {
        float score = std::exp(-std::pow(dist, ALPHA_PENALTY_EXPONENT) * ALPHA_DECAY) * 100.0f;
        return std::max(0.0f, std::min(100.0f, score));
    }
};