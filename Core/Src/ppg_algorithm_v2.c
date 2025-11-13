/**
 * @file ppg_algorithm_v2.c
 * @brief DPT (Discrete Period Transform) based heart rate and SpO2 algorithm implementation
 * @details Based on Analog Devices RAQ-230 paper
 * @author Claude Code
 * @date 2025-01
 * @version 2.0
 */

#include "ppg_algorithm_v2.h"
#include <math.h>
#include <string.h>
#ifndef __ARM_ARCH
#include <time.h>
#endif

/* Performance measurement using DWT cycle counter */
#ifdef __ARM_ARCH
#define DWT_CYCCNT     (*(volatile uint32_t *)0xE0001004U)
#define DWT_CONTROL    (*(volatile uint32_t *)0xE0001000U)
#define DWT_CTRL_CYCCNTENA  (1U << 0)

static inline void dwt_init(void) {
    DWT_CONTROL |= DWT_CTRL_CYCCNTENA;
}

static inline uint32_t dwt_get_cycles(void) {
    return DWT_CYCCNT;
}
#else
// Host system stubs
static inline void dwt_init(void) {
    // No-op on host system
}

static inline uint32_t dwt_get_cycles(void) {
    // Return clock_gettime based measurement on host
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000000LL + ts.tv_nsec); // nanoseconds
}
#endif

/* ==================== Private Constants ==================== */

#define PI                      3.14159265358979323846f
#define TWO_PI                  (2.0f * PI)

// IIR filter coefficients (time constant ~1 second)
#define IIR_HP_COEFF            0.99f       // High-pass for AC
#define IIR_LP_COEFF            0.99f       // Low-pass for DC

// SpO2 calibration coefficients (from paper, Equation 6)
#define SPO2_COEFF_A            (-45.06f)
#define SPO2_COEFF_B            30.354f
#define SPO2_COEFF_C            94.845f

// Validity thresholds
#define MIN_SPO2                70.0f
#define MAX_SPO2                100.0f
#define MIN_HEART_RATE          30.0f
#define MAX_HEART_RATE          150.0f
#define MIN_DC_VALUE            10000       // Minimum DC for valid signal (raised for MAX30102)
#define MIN_PEAK_MAGNITUDE      0.5f        // Minimum spectrum peak (lowered after normalization)

/* ==================== Private Function Prototypes ==================== */

static void iir_filter_init(DPT_IIR_State_t *filter);
static void iir_filter_process(DPT_IIR_State_t *filter, int32_t raw_value);
static void dpt_transform_init(DPT_Transform_t *dpt);
static void dpt_transform_process(DPT_Transform_t *dpt, int32_t ac_value,
                                  const float *cos_basis, const float *sin_basis);
static void compute_magnitude_spectrum(DPT_Transform_t *dpt);
static uint16_t find_peak_period(const DPT_Transform_t *dpt);
static float smooth_array(const float *data, uint8_t size);
static float median_filter(float *data, uint8_t size);
static void precompute_basis_functions(DPT_State_t *state);

/* ==================== Public Function Implementations ==================== */

/**
 * @brief Initialize DPT algorithm state
 */
void DPT_Init(DPT_State_t *state)
{
    if (state == NULL) return;

    // Clear entire structure
    memset(state, 0, sizeof(DPT_State_t));

    // Initialize IIR filters
    iir_filter_init(&state->red_filter);
    iir_filter_init(&state->ir_filter);

    // Initialize DPT transforms
    dpt_transform_init(&state->red_dpt);
    dpt_transform_init(&state->ir_dpt);

    // Precompute basis functions
    precompute_basis_functions(state);

    // Initialize results
    state->heart_rate = 0.0f;
    state->spo2 = 0.0f;
    state->peak_period = 0;
    state->hr_valid = false;
    state->spo2_valid = false;
}

/**
 * @brief Process one sample of red and IR data
 */
void DPT_Process(DPT_State_t *state, uint32_t raw_red, uint32_t raw_ir)
{
    if (state == NULL) return;

    // Start performance measurement
    uint32_t start_cycles = dwt_get_cycles();

    // Step 1: Extract AC and DC components using IIR filters
    iir_filter_process(&state->red_filter, (int32_t)raw_red);
    iir_filter_process(&state->ir_filter, (int32_t)raw_ir);

    // Step 2: Perform DPT transform on AC signals
    dpt_transform_process(&state->red_dpt, state->red_filter.ac_value,
                         state->cos_basis, state->sin_basis);
    dpt_transform_process(&state->ir_dpt, state->ir_filter.ac_value,
                         state->cos_basis, state->sin_basis);

    // Step 3: Check if buffer is full before computing results
    if (!state->red_dpt.buffer_full || !state->ir_dpt.buffer_full) {
        state->hr_valid = false;
        state->spo2_valid = false;
        return;
    }

    // Step 4: Compute magnitude spectrums
    compute_magnitude_spectrum(&state->red_dpt);
    compute_magnitude_spectrum(&state->ir_dpt);

    // Step 5: Find peak period in IR spectrum (dominant signal)
    state->peak_period = find_peak_period(&state->ir_dpt);

    // Step 6: Calculate heart rate from peak period with enhanced smoothing
    // HR (bpm) = 6000 / peak_period (in 10ms intervals)
    if (state->peak_period > 0) {
        float raw_hr = 6000.0f / (float)state->peak_period;

        // Validate raw heart rate range
        if (raw_hr >= MIN_HEART_RATE && raw_hr <= MAX_HEART_RATE) {

            // 1. Add to median filter buffer
            state->hr_median_buffer[state->hr_median_index] = raw_hr;
            state->hr_median_index = (state->hr_median_index + 1) % DPT_MEDIAN_SIZE;

            // 2. Apply median filter (7-point)
            float median_hr = median_filter(state->hr_median_buffer, DPT_MEDIAN_SIZE);

            // 3. Rate limiting: prevent large jumps
            if (state->last_valid_hr > 0.0f) {
                float hr_diff = median_hr - state->last_valid_hr;
                if (hr_diff > DPT_MAX_HR_CHANGE) {
                    median_hr = state->last_valid_hr + DPT_MAX_HR_CHANGE;
                } else if (hr_diff < -DPT_MAX_HR_CHANGE) {
                    median_hr = state->last_valid_hr - DPT_MAX_HR_CHANGE;
                }
            }

            // 4. EMA smoothing
            if (state->ema_hr == 0.0f) {
                // First valid reading
                state->ema_hr = median_hr;
            } else {
                // Apply EMA: alpha * new + (1-alpha) * old
                state->ema_hr = DPT_HR_EMA_ALPHA * median_hr +
                               (1.0f - DPT_HR_EMA_ALPHA) * state->ema_hr;
            }

            // 5. Additional smoothing using hr_history buffer
            state->hr_history[state->hr_index] = state->ema_hr;
            state->hr_index = (state->hr_index + 1) % DPT_HR_SMOOTH_SIZE;
            float smoothed_hr = smooth_array(state->hr_history, DPT_HR_SMOOTH_SIZE);

            // 6. Stability validation: check if change is small (before updating last_valid_hr)
            float change = 0.0f;
            if (state->last_valid_hr > 0.0f) {
                change = fabsf(smoothed_hr - state->last_valid_hr);
            }
            if (change < 3.0f) {  // Change less than 3 bpm
                state->stable_count++;
            } else {
                state->stable_count = 0;
            }

            // 7. Update final heart rate and last valid
            state->heart_rate = smoothed_hr;
            state->last_valid_hr = smoothed_hr;

            // 8. Mark as valid if stable for at least 2 readings
            state->hr_valid = (state->stable_count >= 2);
        } else {
            // Reset HR validation state on invalid range
            state->hr_valid = false;
            state->stable_count = 0;
            state->ema_hr = 0.0f;  // Reset EMA to force re-initialization
        }
    } else {
        // Reset HR validation state on no peak found
        state->hr_valid = false;
        state->stable_count = 0;
        state->ema_hr = 0.0f;  // Reset EMA to force re-initialization
    }

    // Step 7: Calculate SpO2 using R-value method
    if (state->peak_period > 0 &&
        state->red_filter.dc_value > MIN_DC_VALUE &&
        state->ir_filter.dc_value > MIN_DC_VALUE) {

        // Get AC magnitudes from spectrum peaks
        uint16_t peak_idx = state->peak_period - DPT_MIN_PERIOD;
        if (peak_idx < DPT_PERIOD_RANGE) {
            float red_ac = state->red_dpt.magnitude[peak_idx];
            float ir_ac = state->ir_dpt.magnitude[peak_idx];

            // Calculate R value: R = (AC_red/DC_red) / (AC_ir/DC_ir)
            float red_ratio = red_ac / (float)state->red_filter.dc_value;
            float ir_ratio = ir_ac / (float)state->ir_filter.dc_value;

            if (ir_ratio > 0.0f) {
                float r_value = red_ratio / ir_ratio;

                // Add to smoothing buffer
                state->r_history[state->r_index] = r_value;
                state->r_index = (state->r_index + 1) % DPT_R_SMOOTH_SIZE;

                // Smooth R value
                float r_smooth = smooth_array(state->r_history, DPT_R_SMOOTH_SIZE);

                // Calculate SpO2: SpO2 = -45.06*R^2 + 30.354*R + 94.845
                state->spo2 = SPO2_COEFF_A * r_smooth * r_smooth +
                             SPO2_COEFF_B * r_smooth +
                             SPO2_COEFF_C;

                // Validate SpO2
                state->spo2_valid = (state->spo2 >= MIN_SPO2 &&
                                    state->spo2 <= MAX_SPO2);
            } else {
                state->spo2_valid = false;
                // Reset R-value history on invalid IR ratio
                memset(state->r_history, 0, sizeof(state->r_history));
                state->r_index = 0;
            }
        } else {
            // Reset R-value history on invalid peak index
            state->spo2_valid = false;
            memset(state->r_history, 0, sizeof(state->r_history));
            state->r_index = 0;
        }
    } else {
        // Reset R-value history on insufficient signal
        state->spo2_valid = false;
        memset(state->r_history, 0, sizeof(state->r_history));
        state->r_index = 0;
    }

    // End performance measurement
    state->last_process_cycles = dwt_get_cycles() - start_cycles;
}

/**
 * @brief Get calculated heart rate
 */
float DPT_GetHeartRate(const DPT_State_t *state)
{
    if (state == NULL || !state->hr_valid) return 0.0f;
    return state->heart_rate;
}

/**
 * @brief Get calculated SpO2
 */
float DPT_GetSpO2(const DPT_State_t *state)
{
    if (state == NULL || !state->spo2_valid) return 0.0f;
    return state->spo2;
}

/**
 * @brief Check if heart rate is valid
 */
bool DPT_IsHeartRateValid(const DPT_State_t *state)
{
    if (state == NULL) return false;
    return state->hr_valid;
}

/**
 * @brief Check if SpO2 is valid
 */
bool DPT_IsSpO2Valid(const DPT_State_t *state)
{
    if (state == NULL) return false;
    return state->spo2_valid;
}

/**
 * @brief Get the magnitude spectrum
 */
const float* DPT_GetSpectrum(const DPT_State_t *state, uint8_t channel)
{
    if (state == NULL) return NULL;

    if (channel == 0) {
        return state->red_dpt.magnitude;
    } else {
        return state->ir_dpt.magnitude;
    }
}

/**
 * @brief Get the peak period index
 */
uint16_t DPT_GetPeakPeriod(const DPT_State_t *state)
{
    if (state == NULL) return 0;
    return state->peak_period;
}

/**
 * @brief Get CPU cycles for last process call
 */
uint32_t DPT_GetProcessCycles(const DPT_State_t *state)
{
    if (state == NULL) return 0;
    return state->last_process_cycles;
}

/**
 * @brief Initialize DWT cycle counter for performance measurement
 */
void DPT_InitPerformance(void)
{
    dwt_init();
}

/**
 * @brief Get DC values for debugging (test only)
 */
void DPT_GetDebugDC(const DPT_State_t *state, float *red_dc, float *ir_dc)
{
    if (state == NULL || red_dc == NULL || ir_dc == NULL) return;
    
    *red_dc = (float)state->red_filter.dc_value;
    *ir_dc = (float)state->ir_filter.dc_value;
}

/* ==================== Private Function Implementations ==================== */

/**
 * @brief Initialize IIR filter state
 */
static void iir_filter_init(DPT_IIR_State_t *filter)
{
    if (filter == NULL) return;
    memset(filter, 0, sizeof(DPT_IIR_State_t));
}

/**
 * @brief Process one sample through IIR filters to extract AC and DC
 * @details Implements the filters from Figure 14 of the paper
 */
static void iir_filter_process(DPT_IIR_State_t *filter, int32_t raw_value)
{
    if (filter == NULL) return;

    float input = (float)raw_value;

    // High-pass IIR filter to extract AC signal
    // w = (float)RD_in + 0.99*wn
    // RD_ac = -(int32_t)(w - wn)  // 注意负号！
    // wn = w
    float w = input + IIR_HP_COEFF * filter->w_n;
    filter->ac_value = -(int32_t)(w - filter->w_n);  // 添加负号
    filter->w_n = w;

    // Low-pass IIR filter to extract DC signal
    // RD_dc = (int32_t)(0.99*yn + 0.01*(float)RD_in)
    // yn = (float)RD_dc
    filter->y_n = IIR_LP_COEFF * filter->y_n + (1.0f - IIR_LP_COEFF) * input;
    filter->dc_value = (int32_t)filter->y_n;
}

/**
 * @brief Initialize DPT transform state
 */
static void dpt_transform_init(DPT_Transform_t *dpt)
{
    if (dpt == NULL) return;
    memset(dpt, 0, sizeof(DPT_Transform_t));
}

/**
 * @brief Precompute basis functions for all periods
 * @details Computes cos and sin basis function incremental phase angles
 *          Based on sliding window DPT: T_new = e^(-j*2*pi/period) * (T_old - x_old + x_new)
 */
static void precompute_basis_functions(DPT_State_t *state)
{
    if (state == NULL) return;

    for (uint16_t period_idx = 0; period_idx < DPT_PERIOD_RANGE; period_idx++) {
        uint16_t period = DPT_MIN_PERIOD + period_idx;

        // Phase increment for this period: -2*pi / period (注意负号)
        // 使用负号是因为窗口向前滑动时相位向后旋转
        float phase_increment = -TWO_PI / (float)period;

        state->cos_basis[period_idx] = cosf(phase_increment);
        state->sin_basis[period_idx] = sinf(phase_increment);
    }
}

/**
 * @brief Process one sample through DPT transform
 * @details Implements sliding DPT: T_new = e^(-j*2*pi/period) * (T_old - x_old + x_new)
 */
static void dpt_transform_process(DPT_Transform_t *dpt, int32_t ac_value,
                                  const float *cos_basis, const float *sin_basis)
{
    if (dpt == NULL || cos_basis == NULL || sin_basis == NULL) return;

    // Add new sample to circular buffer
    dpt->recursive_buffer[dpt->buffer_index] = ac_value;
    uint16_t current_idx = dpt->buffer_index;
    dpt->buffer_index = (dpt->buffer_index + 1) % DPT_BUFFER_SIZE;
    dpt->sample_count++;

    // Check if buffer is full
    if (dpt->sample_count >= DPT_BUFFER_SIZE) {
        dpt->buffer_full = true;
    }

    // Only perform DPT when buffer has enough data
    if (!dpt->buffer_full) return;

    float new_sample = (float)ac_value;

    // Update DPT for each period
    for (uint16_t period_idx = 0; period_idx < DPT_PERIOD_RANGE; period_idx++) {
        uint16_t period = DPT_MIN_PERIOD + period_idx;

        // Get old sample that's being removed (period samples ago)
        // current_idx points to the sample we just wrote
        // The sample being removed is exactly 'period' samples before current_idx
        uint16_t old_idx = (current_idx + DPT_BUFFER_SIZE - period) % DPT_BUFFER_SIZE;
        float old_sample = (float)dpt->recursive_buffer[old_idx];

        // Complex rotation using basis functions
        // T_new = e^(-j*2*pi/period) * [T_old - x_old + x_new]
        float cos_val = cos_basis[period_idx];
        float sin_val = sin_basis[period_idx];

        // Previous DPT state
        float real_prev = dpt->real[period_idx];
        float imag_prev = dpt->imag[period_idx];

        // Update: subtract old sample, add new sample
        // Since samples are real numbers, only real part is affected
        float real_updated = real_prev - old_sample + new_sample;
        float imag_updated = imag_prev;

        // Apply complex rotation: multiply by e^(-j*2*pi/period)
        // Real part: real*cos - imag*sin
        // Imag part: real*sin + imag*cos
        dpt->real[period_idx] = real_updated * cos_val - imag_updated * sin_val;
        dpt->imag[period_idx] = real_updated * sin_val + imag_updated * cos_val;
    }
}

/**
 * @brief Compute magnitude spectrum from real and imaginary parts
 * @details Normalizes by period length for consistent amplitude across periods
 */
static void compute_magnitude_spectrum(DPT_Transform_t *dpt)
{
    if (dpt == NULL) return;

    for (uint16_t i = 0; i < DPT_PERIOD_RANGE; i++) {
        float real = dpt->real[i];
        float imag = dpt->imag[i];
        uint16_t period = DPT_MIN_PERIOD + i;

        // Calculate magnitude and normalize by period length
        // This ensures consistent amplitude across different periods
        float magnitude_raw = sqrtf(real * real + imag * imag);
        dpt->magnitude[i] = magnitude_raw / (float)period;
    }
}

/**
 * @brief Find peak period in magnitude spectrum
 * @return Peak period in samples (0 if no valid peak found)
 */
static uint16_t find_peak_period(const DPT_Transform_t *dpt)
{
    if (dpt == NULL) return 0;

    float max_magnitude = 0.0f;
    uint16_t peak_index = 0;
    float median_magnitude = 0.0f;

    // Find maximum in spectrum and collect values for median
    float magnitudes[DPT_PERIOD_RANGE];
    for (uint16_t i = 0; i < DPT_PERIOD_RANGE; i++) {
        magnitudes[i] = dpt->magnitude[i];
        if (dpt->magnitude[i] > max_magnitude) {
            max_magnitude = dpt->magnitude[i];
            peak_index = i;
        }
    }

    // Compute adaptive threshold based on median spectrum energy
    // Simple median calculation for small array
    for (uint16_t i = 0; i < DPT_PERIOD_RANGE - 1; i++) {
        for (uint16_t j = 0; j < DPT_PERIOD_RANGE - i - 1; j++) {
            if (magnitudes[j] > magnitudes[j + 1]) {
                float temp = magnitudes[j];
                magnitudes[j] = magnitudes[j + 1];
                magnitudes[j + 1] = temp;
            }
        }
    }
    median_magnitude = magnitudes[DPT_PERIOD_RANGE / 2];

    // Adaptive threshold: peak must be significantly above median
    float adaptive_threshold = MIN_PEAK_MAGNITUDE + median_magnitude * 0.5f;
    if (adaptive_threshold < MIN_PEAK_MAGNITUDE) {
        adaptive_threshold = MIN_PEAK_MAGNITUDE;
    }

    // Validate peak against adaptive threshold
    if (max_magnitude < adaptive_threshold) {
        return 0;
    }

    // Convert index to actual period
    return DPT_MIN_PERIOD + peak_index;
}

/**
 * @brief Smooth array using simple moving average
 */
static float smooth_array(const float *data, uint8_t size)
{
    if (data == NULL || size == 0) return 0.0f;

    float sum = 0.0f;
    uint8_t count = 0;

    for (uint8_t i = 0; i < size; i++) {
        if (data[i] > 0.0f) {  // Only use valid data
            sum += data[i];
            count++;
        }
    }

    return (count > 0) ? (sum / (float)count) : 0.0f;
}

/**
 * @brief Median filter for outlier rejection
 * @param data Array of data (will be modified during sorting)
 * @param size Size of array
 * @return Median value
 */
static float median_filter(float *data, uint8_t size)
{
    if (data == NULL || size == 0) return 0.0f;

    // Create a temporary array for sorting
    float temp[DPT_MEDIAN_SIZE];
    uint8_t valid_count = 0;

    // Copy valid data (non-zero) to temp array
    for (uint8_t i = 0; i < size; i++) {
        if (data[i] > 0.0f) {
            temp[valid_count++] = data[i];
        }
    }

    if (valid_count == 0) return 0.0f;
    if (valid_count == 1) return temp[0];

    // Simple bubble sort (efficient for small arrays)
    for (uint8_t i = 0; i < valid_count - 1; i++) {
        for (uint8_t j = 0; j < valid_count - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                float swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }

    // Return median
    if (valid_count % 2 == 0) {
        // Even number: average of two middle values
        return (temp[valid_count / 2 - 1] + temp[valid_count / 2]) / 2.0f;
    } else {
        // Odd number: middle value
        return temp[valid_count / 2];
    }
}
