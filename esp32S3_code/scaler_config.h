// =============================================
// Scaler Configuration pour ESP32-S3
// =============================================

#ifndef SCALER_CONFIG_H
#define SCALER_CONFIG_H

#define NUM_FEATURES 7

// Mean values
const float MEAN[NUM_FEATURES] = {6.359441f, -207.342493f, -49.755120f, 0.088399f, 39.240266f, -7.015175f, 65.397988f};

// Standard deviation values
const float STD[NUM_FEATURES] = {75.821776f, 133.243492f, 115.916292f, 725.693461f, 490.793140f, 433.385711f, 3.738106f};

// Normalize function
void normalize(float* input, float* output) {
    for (int i = 0; i < NUM_FEATURES; i++) {
        output[i] = (input[i] - MEAN[i]) / STD[i];
    }
}

#endif
