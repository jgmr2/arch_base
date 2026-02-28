#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "motorestadistico.hpp"
#include "camara.hpp"

using namespace emscripten;

class BiometricEngine : public StatisticalCore {
public:
    BiometricEngine() {}

    FrameAnalysis processFrame(uintptr_t ptr, int numLandmarks) {
        // Casteo explícito a float* para la función de CameraUtils
        return CameraUtils::analyzeAndNormalize(reinterpret_cast<float*>(ptr), numLandmarks);
    }

    float calculateMahalanobis(uintptr_t vecPtr, uintptr_t meanPtr, uintptr_t invPtr, int dims) {
        return this->computeMahalanobis(
            reinterpret_cast<float*>(vecPtr),
            reinterpret_cast<float*>(meanPtr),
            reinterpret_cast<float*>(invPtr),
            dims
        );
    }

    float getConfidenceScore(float dist) {
        return this->computeScore(dist);
    }

    BiometricModel trainModel(uintptr_t ptr, int numSamples, int dims, std::string id) {
        float* allSamples = reinterpret_cast<float*>(ptr);
        BiometricModel model;
        model.userId = id;
        model.mean.assign(dims, 0.0f);
        model.invCovariance.assign(dims, 0.0f);

        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < dims; ++j) model.mean[j] += allSamples[i * dims + j];
        }
        for (int j = 0; j < dims; ++j) model.mean[j] /= numSamples;

        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < dims; ++j) {
                float diff = allSamples[i * dims + j] - model.mean[j];
                model.invCovariance[j] += diff * diff;
            }
        }
        for (int j = 0; j < dims; ++j) {
            model.invCovariance[j] = 1.0f / ((model.invCovariance[j] / numSamples) + RIGOROUS_SMOOTHING);
        }
        
        return model;
    }
};

EMSCRIPTEN_BINDINGS(biometric_module) {
    register_vector<float>("VectorFloat");

    value_object<FrameAnalysis>("FrameAnalysis")
        .field("minX", &FrameAnalysis::minX)
        .field("minY", &FrameAnalysis::minY)
        .field("maxX", &FrameAnalysis::maxX)
        .field("maxY", &FrameAnalysis::maxY)
        .field("yaw", &FrameAnalysis::yaw)
        .field("pitch", &FrameAnalysis::pitch);

    value_object<BiometricModel>("BiometricModel")
        .field("mean", &BiometricModel::mean)
        .field("invCovariance", &BiometricModel::invCovariance)
        .field("userId", &BiometricModel::userId);

    class_<BiometricEngine>("BiometricEngine")
        .constructor<>()
        .function("processFrame", &BiometricEngine::processFrame)
        .function("calculateMahalanobis", &BiometricEngine::calculateMahalanobis)
        .function("getConfidenceScore", &BiometricEngine::getConfidenceScore)
        .function("trainModel", &BiometricEngine::trainModel);
}