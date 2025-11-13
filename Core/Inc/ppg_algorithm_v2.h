/**
 * @file ppg_algorithm_v2.h
 * @brief DPT (Discrete Period Transform) based heart rate and SpO2 algorithm
 * @details Based on Analog Devices RAQ-230 paper:
 *          "A Novel Discrete Period Transform Method for Processing Physiological Signals"
 * @author Claude Code
 * @date 2025-01
 * @version 2.0
 */

#ifndef PPG_ALGORITHM_V2_H
#define PPG_ALGORITHM_V2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ==================== Configuration Parameters ==================== */

// Sampling parameters
#define DPT_SAMPLE_RATE_HZ      100         // 100 Hz sampling rate
#define DPT_SAMPLE_PERIOD_MS    10          // 10 ms per sample

// Period range (in samples)
#define DPT_MIN_PERIOD          40          // 40 samples = 400ms = 150 bpm
#define DPT_MAX_PERIOD          200         // 200 samples = 2000ms = 30 bpm
#define DPT_PERIOD_RANGE        (DPT_MAX_PERIOD - DPT_MIN_PERIOD + 1)

// Recursive buffer size (10 seconds of data)
#define DPT_BUFFER_SIZE         1000        // 10 seconds * 100 Hz

// Smoothing parameters
#define DPT_R_SMOOTH_SIZE       10          // 10-point smoothing for R value
#define DPT_HR_SMOOTH_SIZE      7           // 7-point smoothing for heart rate (increased)
#define DPT_MEDIAN_SIZE         7           // 7-point median filter
#define DPT_HR_EMA_ALPHA        0.15f       // EMA smoothing coefficient for HR
#define DPT_MAX_HR_CHANGE       8.0f        // Maximum HR change per update (bpm)

/* ==================== Data Structures ==================== */

/**
 * @brief IIR filter state for AC/DC extraction
 */
typedef struct {
    float w_n;          // High-pass state for AC
    float y_n;          // Low-pass state for DC (red)
    float x_n;          // High-pass state for AC (IR)
    float z_n;          // Low-pass state for DC (IR)
    int32_t ac_value;   // Current AC value
    int32_t dc_value;   // Current DC value
} DPT_IIR_State_t;

/**
 * @brief DPT transform state for one channel
 */
typedef struct {
    float real[DPT_PERIOD_RANGE];           // Real part of DPT spectrum
    float imag[DPT_PERIOD_RANGE];           // Imaginary part of DPT spectrum
    float magnitude[DPT_PERIOD_RANGE];      // Magnitude spectrum
    int32_t recursive_buffer[DPT_BUFFER_SIZE];  // Circular buffer
    uint16_t buffer_index;                  // Current buffer position
    uint16_t sample_count;                  // Number of samples collected
    bool buffer_full;                       // Buffer filled flag
} DPT_Transform_t;

/**
 * @brief Complete DPT algorithm state
 */
typedef struct {
    // IIR filters for AC/DC extraction
    DPT_IIR_State_t red_filter;
    DPT_IIR_State_t ir_filter;

    // DPT transforms
    DPT_Transform_t red_dpt;
    DPT_Transform_t ir_dpt;

    // Basis functions (precomputed)
    float cos_basis[DPT_PERIOD_RANGE];
    float sin_basis[DPT_PERIOD_RANGE];

    // Results
    float heart_rate;           // Current heart rate (bpm)
    float spo2;                 // Current SpO2 (%)
    uint16_t peak_period;       // Peak period in samples

    // EMA and stability
    float ema_hr;               // EMA smoothed heart rate
    float last_valid_hr;        // Last valid heart rate for rate limiting
    uint8_t stable_count;       // Consecutive stable readings count

    // Smoothing buffers
    float r_history[DPT_R_SMOOTH_SIZE];
    uint8_t r_index;
    float hr_history[DPT_HR_SMOOTH_SIZE];
    uint8_t hr_index;
    float hr_median_buffer[DPT_MEDIAN_SIZE];
    uint8_t hr_median_index;

    // Validity flags
    bool hr_valid;
    bool spo2_valid;

    // Performance metrics
    uint32_t last_process_cycles;  // CPU cycles for last process call

} DPT_State_t;

/* ==================== Function Prototypes ==================== */

/**
 * @brief Initialize DPT algorithm state
 * @param state Pointer to DPT state structure
 */
void DPT_Init(DPT_State_t *state);

/**
 * @brief Process one sample of red and IR data
 * @param state Pointer to DPT state structure
 * @param raw_red Raw red LED ADC value
 * @param raw_ir Raw infrared LED ADC value
 */
void DPT_Process(DPT_State_t *state, uint32_t raw_red, uint32_t raw_ir);

/**
 * @brief Get calculated heart rate
 * @param state Pointer to DPT state structure
 * @return Heart rate in bpm (0 if invalid)
 */
float DPT_GetHeartRate(const DPT_State_t *state);

/**
 * @brief Get calculated SpO2
 * @param state Pointer to DPT state structure
 * @return SpO2 percentage (0 if invalid)
 */
float DPT_GetSpO2(const DPT_State_t *state);

/**
 * @brief Check if heart rate is valid
 * @param state Pointer to DPT state structure
 * @return true if valid, false otherwise
 */
bool DPT_IsHeartRateValid(const DPT_State_t *state);

/**
 * @brief Check if SpO2 is valid
 * @param state Pointer to DPT state structure
 * @return true if valid, false otherwise
 */
bool DPT_IsSpO2Valid(const DPT_State_t *state);

/**
 * @brief Get the magnitude spectrum for visualization
 * @param state Pointer to DPT state structure
 * @param channel 0 for red, 1 for IR
 * @return Pointer to magnitude array (read-only)
 */
const float* DPT_GetSpectrum(const DPT_State_t *state, uint8_t channel);

/**
 * @brief Get the peak period index in spectrum
 * @param state Pointer to DPT state structure
 * @return Peak period in samples
 */
uint16_t DPT_GetPeakPeriod(const DPT_State_t *state);

/**
 * @brief Get CPU cycles for last process call
 * @param state Pointer to DPT state structure
 * @return CPU cycles (0 if never called)
 */
uint32_t DPT_GetProcessCycles(const DPT_State_t *state);

/**
 * @brief Initialize DWT cycle counter for performance measurement
 */
void DPT_InitPerformance(void);

/**
 * @brief Get DC values for debugging (test only)
 * @param state Pointer to DPT state structure
 * @param red_dc Pointer to store red DC value
 * @param ir_dc Pointer to store IR DC value
 */
void DPT_GetDebugDC(const DPT_State_t *state, float *red_dc, float *ir_dc);

#ifdef __cplusplus
}
#endif

#endif /* PPG_ALGORITHM_V2_H */
