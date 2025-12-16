// =============================================================
// 🤖 OPTIMIZED PREDICTION FUNCTION
// Post-processing layer for improved accuracy
// Uses sensor fusion and temporal filtering
// =============================================================

#include <math.h>

static float _acc_buffer[10] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
static float _gyro_buffer[10] = {0};
static int _buffer_idx = 0;
static int _buffer_ready = 0;

#define SF_ACC_IDLE_MIN 0.85f
#define SF_ACC_IDLE_MAX 1.20f
#define SF_GYRO_IDLE_MAX 20.0f
#define SF_ACC_FALL_LOW 0.4f
#define SF_ACC_FALL_HIGH 2.5f
#define SF_GYRO_FALL_MIN 200.0f

/**
 * @brief Optimized prediction with sensor fusion
 * @param ax Accelerometer X (m/s²)
 * @param ay Accelerometer Y (m/s²)
 * @param az Accelerometer Z (m/s²)
 * @param gx Gyroscope X (rad/s)
 * @param gy Gyroscope Y (rad/s)
 * @param gz Gyroscope Z (rad/s)
 * @return Predicted class:  0=IDLE, 1=WALKING, 2=FALLING
 */
int32_t fall_model_predict_optimized(float ax, float ay, float az, 
                                      float gx, float gy, float gz) {
    
    float acc_mag = sqrtf((ax * ax + ay * ay + az * az)) / 9.81f;
    float gyro_mag = sqrtf((gx * gx + gy * gy + gz * gz)) * 57.2958f;
    
    _acc_buffer[_buffer_idx] = acc_mag;
    _gyro_buffer[_buffer_idx] = gyro_mag;
    _buffer_idx = (_buffer_idx + 1) % 10;
    if (_buffer_idx == 0) _buffer_ready = 1;
    
    float acc_avg = 0.0f;
    float gyro_avg = 0.0f;
    int count = _buffer_ready ?  10 : (_buffer_idx > 0 ? _buffer_idx : 1);
    
    for (int i = 0; i < count; i++) {
        acc_avg += _acc_buffer[i];
        gyro_avg += _gyro_buffer[i];
    }
    acc_avg /= (float)count;
    gyro_avg /= (float)count;
    
    
    if (acc_mag < SF_ACC_FALL_LOW || 
        acc_mag > SF_ACC_FALL_HIGH || 
        (gyro_mag > SF_GYRO_FALL_MIN && acc_mag > 1.8f)) {
        return 2;
    }
    

    if (acc_avg > SF_ACC_IDLE_MIN && 
        acc_avg < SF_ACC_IDLE_MAX && 
        gyro_avg < SF_GYRO_IDLE_MAX) {
        return 0;
    }
    
    
    return 1;
}