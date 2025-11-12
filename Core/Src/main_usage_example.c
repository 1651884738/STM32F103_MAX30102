/**
 * @file main_usage_example.c
 * @brief Example code showing how to use both algorithm methods
 * @details This file demonstrates how to switch between:
 *          - Method 1: Time-domain peak detection (ppg_algorithm.h)
 *          - Method 2: DPT frequency-domain analysis (ppg_algorithm_v2.h)
 * @author Claude Code
 * @date 2025-01
 */

#include "main.h"
#include "ppg_algorithm.h"      // Method 1: Peak detection
#include "ppg_algorithm_v2.h"   // Method 2: DPT transform

/* ==================== Method Selection ==================== */

// Uncomment ONE of the following lines to select algorithm method:
#define USE_METHOD_1    // Time-domain peak detection (current default)
// #define USE_METHOD_2    // DPT frequency-domain analysis (new method)

/* ==================== Global Variables ==================== */

#ifdef USE_METHOD_1
    // Method 1 variables
    PPG_FilterState_t red_filter;
    PPG_FilterState_t ir_filter;
    HR_State_t hr_state;
    SpO2_State_t spo2_state;

    // Display smoothing for Method 1
    float displayed_hr = 0.0f;
    #define DISPLAY_EMA_ALPHA 0.1f
    #define DISPLAY_HR_THRESHOLD 2.0f
#endif

#ifdef USE_METHOD_2
    // Method 2 variables
    DPT_State_t dpt_state;

    // Display smoothing for Method 2 (already built into algorithm)
#endif

/* ==================== Initialization ==================== */

void Algorithm_Init(void)
{
#ifdef USE_METHOD_1
    printf("\r\n=== Using Method 1: Time-Domain Peak Detection ===\r\n");

    PPG_Filter_Init(&red_filter);
    PPG_Filter_Init(&ir_filter);
    HR_Init(&hr_state);
    SpO2_Init(&spo2_state);
    displayed_hr = 0.0f;

    printf("Method 1 initialized successfully.\r\n");
#endif

#ifdef USE_METHOD_2
    printf("\r\n=== Using Method 2: DPT Frequency-Domain Analysis ===\r\n");

    DPT_Init(&dpt_state);

    printf("Method 2 initialized successfully.\r\n");
    printf("Buffer size: %d samples (10 seconds)\r\n", DPT_BUFFER_SIZE);
    printf("Period range: %d - %d samples (%d - %d bpm)\r\n",
           DPT_MIN_PERIOD, DPT_MAX_PERIOD,
           (int)(6000.0f / DPT_MAX_PERIOD), (int)(6000.0f / DPT_MIN_PERIOD));
#endif
}

/* ==================== Processing ==================== */

void Algorithm_ProcessSample(uint32_t raw_red, uint32_t raw_ir)
{
#ifdef USE_METHOD_1
    // Method 1: Filter and process
    float filtered_red = PPG_Filter_Process(&red_filter, raw_red);
    float filtered_ir = PPG_Filter_Process(&ir_filter, raw_ir);

    // Add to HR algorithm
    HR_AddSample(&hr_state, filtered_ir);

    // Calculate HR and SpO2
    float heart_rate = HR_Calculate(&hr_state);
    float spo2 = SpO2_Calculate(&spo2_state, &red_filter, &ir_filter);

    // Apply display smoothing
    if (HR_IsValid(&hr_state)) {
        if (displayed_hr == 0.0f) {
            displayed_hr = heart_rate;
        } else {
            float hr_diff = fabsf(heart_rate - displayed_hr);
            if (hr_diff > DISPLAY_HR_THRESHOLD) {
                displayed_hr = DISPLAY_EMA_ALPHA * heart_rate +
                              (1.0f - DISPLAY_EMA_ALPHA) * displayed_hr;
            }
        }
    }
#endif

#ifdef USE_METHOD_2
    // Method 2: Direct DPT processing
    DPT_Process(&dpt_state, raw_red, raw_ir);
#endif
}

/* ==================== Results Retrieval ==================== */

float Algorithm_GetHeartRate(void)
{
#ifdef USE_METHOD_1
    return displayed_hr;
#endif

#ifdef USE_METHOD_2
    return DPT_GetHeartRate(&dpt_state);
#endif
}

float Algorithm_GetSpO2(void)
{
#ifdef USE_METHOD_1
    return SpO2_GetValue(&spo2_state);
#endif

#ifdef USE_METHOD_2
    return DPT_GetSpO2(&dpt_state);
#endif
}

bool Algorithm_IsHeartRateValid(void)
{
#ifdef USE_METHOD_1
    return HR_IsValid(&hr_state);
#endif

#ifdef USE_METHOD_2
    return DPT_IsHeartRateValid(&dpt_state);
#endif
}

bool Algorithm_IsSpO2Valid(void)
{
#ifdef USE_METHOD_1
    return SpO2_IsValid(&spo2_state);
#endif

#ifdef USE_METHOD_2
    return DPT_IsSpO2Valid(&dpt_state);
#endif
}

/* ==================== Display Function ==================== */

void Algorithm_DisplayResults(void)
{
    float hr = Algorithm_GetHeartRate();
    float spo2 = Algorithm_GetSpO2();
    bool hr_valid = Algorithm_IsHeartRateValid();
    bool spo2_valid = Algorithm_IsSpO2Valid();

    // Display on OLED
    char str[32];

    if (hr_valid) {
        snprintf(str, sizeof(str), "HR: %d bpm", (int)hr);
    } else {
        snprintf(str, sizeof(str), "HR: ---");
    }
    OLED_ShowString(0, 0, (uint8_t*)str, 12, 1);

    if (spo2_valid) {
        snprintf(str, sizeof(str), "SpO2: %d%%", (int)spo2);
    } else {
        snprintf(str, sizeof(str), "SpO2: --%%");
    }
    OLED_ShowString(64, 0, (uint8_t*)str, 12, 1);

    // Print to UART for debugging
#ifdef USE_METHOD_1
    printf("Method1 | HR: %d bpm | SpO2: %d%% | Valid: HR=%d, SpO2=%d\r\n",
           (int)hr, (int)spo2, hr_valid, spo2_valid);
#endif

#ifdef USE_METHOD_2
    uint16_t peak_period = DPT_GetPeakPeriod(&dpt_state);
    printf("Method2 | HR: %d bpm | SpO2: %d%% | Peak Period: %d samples | Valid: HR=%d, SpO2=%d\r\n",
           (int)hr, (int)spo2, peak_period, hr_valid, spo2_valid);
#endif
}

/* ==================== Method Comparison Function ==================== */

/**
 * @brief Advanced example: Run both methods simultaneously for comparison
 * @note This requires more RAM and CPU, use carefully
 */
void Algorithm_RunComparison(uint32_t raw_red, uint32_t raw_ir)
{
    // Method 1 processing
    static PPG_FilterState_t red_filter_m1, ir_filter_m1;
    static HR_State_t hr_state_m1;
    static SpO2_State_t spo2_state_m1;
    static bool method1_initialized = false;

    if (!method1_initialized) {
        PPG_Filter_Init(&red_filter_m1);
        PPG_Filter_Init(&ir_filter_m1);
        HR_Init(&hr_state_m1);
        SpO2_Init(&spo2_state_m1);
        method1_initialized = true;
    }

    float filtered_red = PPG_Filter_Process(&red_filter_m1, raw_red);
    float filtered_ir = PPG_Filter_Process(&ir_filter_m1, raw_ir);
    HR_AddSample(&hr_state_m1, filtered_ir);
    float hr1 = HR_Calculate(&hr_state_m1);
    float spo2_1 = SpO2_Calculate(&spo2_state_m1, &red_filter_m1, &ir_filter_m1);

    // Method 2 processing
    static DPT_State_t dpt_state_m2;
    static bool method2_initialized = false;

    if (!method2_initialized) {
        DPT_Init(&dpt_state_m2);
        method2_initialized = true;
    }

    DPT_Process(&dpt_state_m2, raw_red, raw_ir);
    float hr2 = DPT_GetHeartRate(&dpt_state_m2);
    float spo2_2 = DPT_GetSpO2(&dpt_state_m2);

    // Print comparison
    printf("Comparison | M1: HR=%d SpO2=%d | M2: HR=%d SpO2=%d | Diff: HR=%d SpO2=%d\r\n",
           (int)hr1, (int)spo2_1,
           (int)hr2, (int)spo2_2,
           (int)(hr1 - hr2), (int)(spo2_1 - spo2_2));
}

/* ==================== Usage Example in main() ==================== */

#if 0  // Example code, not compiled
int main(void)
{
    // System initialization...
    HAL_Init();
    SystemClock_Config();
    // ... peripheral init ...

    // Initialize selected algorithm
    Algorithm_Init();

    while (1)
    {
        // Read MAX30102 sensor
        uint32_t red, ir;
        if (MAX30102_ReadFIFO(&red, &ir) == 0) {

            // Process with selected method
            Algorithm_ProcessSample(red, ir);

            // Display results periodically
            static uint32_t display_counter = 0;
            if (++display_counter >= 250) {  // Every 2.5 seconds
                display_counter = 0;
                Algorithm_DisplayResults();
                OLED_Refresh();
            }
        }

        HAL_Delay(10);
    }
}
#endif

/**
 * ==================== Key Differences Between Methods ====================
 *
 * Method 1 (ppg_algorithm.h):
 * - Time-domain peak detection
 * - Butterworth bandpass filtering (0.5-4 Hz)
 * - 7-point median filtering
 * - Adaptive threshold peak detection
 * - ~5 second initialization time
 * - Lower memory usage (~2 KB)
 * - Faster per-sample processing
 *
 * Method 2 (ppg_algorithm_v2.h):
 * - Frequency-domain DPT analysis
 * - IIR filtering for AC/DC extraction
 * - Period domain spectrum analysis
 * - Direct spectrum peak detection
 * - ~10 second initialization time (buffer fill)
 * - Higher memory usage (~8 KB)
 * - More computational intensive (DPT transform)
 * - Potentially more robust to noise
 * - Based on Analog Devices research paper
 *
 * How to Choose:
 * - Use Method 1 for faster response and lower memory
 * - Use Method 2 for potentially better noise immunity and spectral analysis
 * - Run comparison mode to validate both methods on your specific application
 */
