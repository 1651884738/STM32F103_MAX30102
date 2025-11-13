/**
 * @file dpt_method_test.c
 * @brief Unit tests for DPT-based HR/SpO2 algorithm
 * @details Tests synthetic signals with known HR/SpO2 values
 * @author Claude Code
 * @date 2025-01
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>

// Include the algorithm header
#include "ppg_algorithm_v2.h"

/* Test Configuration */
#define TEST_SAMPLE_RATE        100     // Hz
#define TEST_DURATION_SEC       30      // seconds per test
#define TEST_SAMPLES            (TEST_DURATION_SEC * TEST_SAMPLE_RATE)
#define CONVERGENCE_SAMPLES     200     // samples to wait for convergence
#define HR_TOLERANCE_BPM        2.0f    // ±2 bpm tolerance
#define SPO2_TOLERANCE_PERCENT  2.0f    // ±2% tolerance

/* Test Cases */
typedef struct {
    float heart_rate;       // Target heart rate (bpm)
    float spo2;            // Target SpO2 (%)
    float signal_strength; // Signal amplitude factor
    const char* name;      // Test case name
} TestCase_t;

/* Synthetic signal generation */
static void generate_ppg_signal(uint32_t* red_samples, uint32_t* ir_samples, 
                                const TestCase_t* test_case)
{
    float heart_rate_hz = test_case->heart_rate / 60.0f;
    float spo2_ratio = test_case->spo2;
    
    // Convert SpO2 to AC/DC ratio (approximate)
    // Higher SpO2 = lower AC/DC ratio for IR compared to Red
    // Based on typical R values: SpO2=98% -> R=0.4, SpO2=95% -> R=0.8, SpO2=88% -> R=1.2
    float red_ac_ratio = 0.02f * test_case->signal_strength;
    
    // Calculate target R value from SpO2 using inverse of the calibration formula
    float target_r;
    if (spo2_ratio >= 95.0f) {
        target_r = (100.0f - spo2_ratio) / 10.0f;  // Approx: 98% -> 0.2, 95% -> 0.5
    } else {
        target_r = (100.0f - spo2_ratio) / 10.0f;  // Approx: 92% -> 0.8, 88% -> 1.2
    }
    
    // IR AC ratio = Red AC ratio / R value
    float ir_ac_ratio = red_ac_ratio / target_r;
    
    // Base DC values (typical for MAX30102)
    uint32_t red_dc = 50000;
    uint32_t ir_dc = 80000;
    
    printf("  Debug: Signal generation - DC values: Red=%u, IR=%u\n", red_dc, ir_dc);
    printf("  Debug: Target HR=%.1f -> period=%.1f samples\n", 
           test_case->heart_rate, 6000.0f / test_case->heart_rate);
    
    for (int i = 0; i < TEST_SAMPLES; i++) {
        float t = (float)i / TEST_SAMPLE_RATE;
        
        // Generate sinusoidal PPG signal with slight noise
        float phase = 2.0f * M_PI * heart_rate_hz * t;
        float noise = ((float)rand() / RAND_MAX - 0.5f) * 0.1f; // ±10% noise
        
        // Red channel
        float red_ac = red_dc * red_ac_ratio * sinf(phase) * (1.0f + noise);
        red_samples[i] = (uint32_t)(red_dc + red_ac);
        
        // IR channel (different AC/DC ratio based on SpO2)
        float ir_ac = ir_dc * ir_ac_ratio * sinf(phase) * (1.0f + noise);
        ir_samples[i] = (uint32_t)(ir_dc + ir_ac);
    }
}

/* Test convergence to target values */
static bool test_convergence(const TestCase_t* test_case)
{
    printf("Testing %s (HR=%.1f bpm, SpO2=%.1f%%)...\n", 
           test_case->name, test_case->heart_rate, test_case->spo2);
    
    // Allocate sample buffers
    uint32_t* red_samples = malloc(TEST_SAMPLES * sizeof(uint32_t));
    uint32_t* ir_samples = malloc(TEST_SAMPLES * sizeof(uint32_t));
    
    if (!red_samples || !ir_samples) {
        printf("  FAIL: Memory allocation failed\n");
        return false;
    }
    
    // Generate synthetic signal
    generate_ppg_signal(red_samples, ir_samples, test_case);
    
    // Initialize algorithm
    DPT_InitPerformance();  // Initialize DWT counter
    DPT_State_t state;
    DPT_Init(&state);
    
    // Process samples
    bool hr_converged = false;
    bool spo2_converged = false;
    float hr_sum = 0.0f;
    float spo2_sum = 0.0f;
    int hr_valid_count = 0;
    int spo2_valid_count = 0;
    int both_valid_count = 0;
    int peak_found_count = 0;
    
    for (int i = 0; i < TEST_SAMPLES; i++) {
        DPT_Process(&state, red_samples[i], ir_samples[i]);
        
        // Debug: Check if peaks are being found
        if (i >= CONVERGENCE_SAMPLES && DPT_GetPeakPeriod(&state) > 0) {
            peak_found_count++;
        }
        
        // Check convergence after initial warmup
        if (i >= CONVERGENCE_SAMPLES) {
            if (DPT_IsHeartRateValid(&state)) {
                hr_sum += DPT_GetHeartRate(&state);
                hr_valid_count++;
                hr_converged = true;
            }
            
            if (DPT_IsSpO2Valid(&state)) {
                spo2_sum += DPT_GetSpO2(&state);
                spo2_valid_count++;
                spo2_converged = true;
            }
            
            if (DPT_IsHeartRateValid(&state) && DPT_IsSpO2Valid(&state)) {
                both_valid_count++;
            }
        }
    }
    
    printf("  Debug: Peaks found in %d/%d samples after warmup\n", 
           peak_found_count, TEST_SAMPLES - CONVERGENCE_SAMPLES);
    
    // Debug: Check final peak period
    uint16_t final_peak_period = DPT_GetPeakPeriod(&state);
    float calculated_hr = final_peak_period > 0 ? 6000.0f / (float)final_peak_period : 0.0f;
    printf("  Debug: Final peak period = %u samples, calculated HR = %.1f bpm\n", 
           final_peak_period, calculated_hr);
    
    // Debug SpO2 calculation - let's manually check the components
    // Get the spectrum to see AC values
    const float* red_spectrum = DPT_GetSpectrum(&state, 0);
    const float* ir_spectrum = DPT_GetSpectrum(&state, 1);
    
    if (final_peak_period >= 40 && final_peak_period <= 200) {
        uint16_t peak_idx = final_peak_period - 40;
        float red_ac = red_spectrum[peak_idx];
        float ir_ac = ir_spectrum[peak_idx];
        
        printf("  Debug: Peak idx=%u, Red AC=%.3f, IR AC=%.3f\n", 
               peak_idx, red_ac, ir_ac);
    
    // Get DC values for debugging
    float red_dc, ir_dc;
    DPT_GetDebugDC(&state, &red_dc, &ir_dc);
    printf("  Debug: DC values - Red=%.1f, IR=%.1f\n", red_dc, ir_dc);
    
    // Manual SpO2 calculation to debug
    if (final_peak_period >= 40 && final_peak_period <= 200 && red_dc > 10000 && ir_dc > 10000) {
        uint16_t peak_idx = final_peak_period - 40;
        float red_ac = red_spectrum[peak_idx];
        float ir_ac = ir_spectrum[peak_idx];
        
        float red_ratio = red_ac / red_dc;
        float ir_ratio = ir_ac / ir_dc;
        
        printf("  Debug: Ratios - Red=%.6f, IR=%.6f\n", red_ratio, ir_ratio);
        
        if (ir_ratio > 0.0f) {
            float r_value = red_ratio / ir_ratio;
            printf("  Debug: R value = %.6f\n", r_value);
            
            // Manual SpO2 calculation
            float manual_spo2 = -45.06f * r_value * r_value + 30.354f * r_value + 94.845f;
            printf("  Debug: Manual SpO2 = %.2f\n", manual_spo2);
        }
    }
    }
    
    printf("  Debug: HR valid = %s, SpO2 valid = %s, final SpO2 = %.1f\n",
           DPT_IsHeartRateValid(&state) ? "true" : "false",
           DPT_IsSpO2Valid(&state) ? "true" : "false",
           DPT_GetSpO2(&state));
    
    // Calculate averages - use separate counts for HR and SpO2
    int total_samples = TEST_SAMPLES - CONVERGENCE_SAMPLES;
    float avg_hr = (hr_valid_count > 0) ? hr_sum / hr_valid_count : 0.0f;
    float avg_spo2 = (spo2_valid_count > 0) ? spo2_sum / spo2_valid_count : 0.0f;
    
    // Check results
    bool hr_pass = hr_converged && (fabsf(avg_hr - test_case->heart_rate) <= HR_TOLERANCE_BPM);
    bool spo2_pass = spo2_converged && (fabsf(avg_spo2 - test_case->spo2) <= SPO2_TOLERANCE_PERCENT);
    bool valid_rate_pass = (both_valid_count >= total_samples * 0.8f); // 80% both valid rate
    
    printf("  Results: HR=%.1f bpm (target %.1f), SpO2=%.1f%% (target %.1f), Valid rate=%.1f%%\n",
           avg_hr, test_case->heart_rate, avg_spo2, test_case->spo2,
           (float)both_valid_count / (TEST_SAMPLES - CONVERGENCE_SAMPLES) * 100.0f);
    
    printf("  Status: HR %s, SpO2 %s, Valid rate %s\n",
           hr_pass ? "PASS" : "FAIL",
           spo2_pass ? "PASS" : "FAIL", 
           valid_rate_pass ? "PASS" : "FAIL");
    
    bool overall_pass = hr_pass && spo2_pass && valid_rate_pass;
    printf("  Overall: %s\n\n", overall_pass ? "PASS" : "FAIL");
    
    // Cleanup
    free(red_samples);
    free(ir_samples);
    
    return overall_pass;
}

/* Test performance characteristics */
static bool test_performance(void)
{
    printf("Testing performance characteristics...\n");
    
    DPT_State_t state;
    DPT_Init(&state);
    
    uint32_t total_cycles = 0;
    uint32_t max_cycles = 0;
    uint32_t min_cycles = UINT32_MAX;
    
    // Process 1000 samples to get statistics
    for (int i = 0; i < 1000; i++) {
        DPT_Process(&state, 50000 + i, 80000 + i);
        
        uint32_t cycles = DPT_GetProcessCycles(&state);
        if (cycles > 0) {
            total_cycles += cycles;
            if (cycles > max_cycles) max_cycles = cycles;
            if (cycles < min_cycles) min_cycles = cycles;
        }
    }
    
    uint32_t avg_cycles = total_cycles / 1000;
    
    printf("  Average cycles per sample: %u\n", avg_cycles);
    printf("  Min cycles: %u\n", min_cycles);
    printf("  Max cycles: %u\n", max_cycles);
    
    // Assuming 72MHz clock (typical for STM32F103)
    float max_cpu_percent = (float)max_cycles / 72000000.0f * 100.0f;
    float avg_cpu_percent = (float)avg_cycles / 72000000.0f * 100.0f;
    
    printf("  Average CPU usage: %.2f%%\n", avg_cpu_percent);
    printf("  Peak CPU usage: %.2f%%\n", max_cpu_percent);
    
    bool cpu_ok = max_cpu_percent < 40.0f;  // 40% budget requirement
    printf("  CPU usage within 40%% budget: %s\n\n", cpu_ok ? "PASS" : "FAIL");
    
    return cpu_ok;
}

/* Test buffer reset on invalid data */
static bool test_invalid_data_reset(void)
{
    printf("Testing buffer reset on invalid data...\n");
    
    DPT_State_t state;
    DPT_Init(&state);
    
    // Feed valid data first
    for (int i = 0; i < 500; i++) {
        float t = (float)i / TEST_SAMPLE_RATE;
        float phase = 2.0f * M_PI * 1.2f * t; // 72 bpm
        uint32_t red = 50000 + (uint32_t)(1000.0f * sinf(phase));
        uint32_t ir = 80000 + (uint32_t)(800.0f * sinf(phase));
        DPT_Process(&state, red, ir);
    }
    
    bool was_valid = DPT_IsHeartRateValid(&state);
    uint8_t old_stable_count = state.stable_count;
    
    printf("  After valid data: HR valid=%s, stable_count=%u\n",
           was_valid ? "true" : "false", old_stable_count);
    
    // Feed invalid data (signal loss)
    for (int i = 0; i < 50; i++) {
        DPT_Process(&state, 100, 100); // Very low DC values
    }
    
    bool is_valid = DPT_IsHeartRateValid(&state);
    uint8_t new_stable_count = state.stable_count;
    
    printf("  After invalid data: HR valid=%s, stable_count=%u\n",
           is_valid ? "true" : "false", new_stable_count);
    
    bool reset_ok = !is_valid && (new_stable_count == 0);
    printf("  Reset behavior: %s\n\n", reset_ok ? "PASS" : "FAIL");
    
    return reset_ok;
}

/* Main test runner */
int main(void)
{
    printf("=== DPT Method 2 Unit Tests ===\n\n");
    
    // Define test cases covering typical ranges
    TestCase_t test_cases[] = {
        {60.0f, 98.0f, 1.0f, "Normal resting"},
        {80.0f, 95.0f, 1.0f, "Moderate heart rate"},
        {120.0f, 92.0f, 1.0f, "Elevated heart rate"},
        {45.0f, 97.0f, 0.8f, "Low heart rate"},
        {100.0f, 88.0f, 0.6f, "Low SpO2"},
        {75.0f, 100.0f, 1.2f, "High SpO2"}
    };
    
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    
    // Run convergence tests
    for (int i = 0; i < num_tests; i++) {
        if (test_convergence(&test_cases[i])) {
            passed_tests++;
        }
    }
    
    // Run performance test
    bool performance_pass = test_performance();
    if (performance_pass) passed_tests++;
    
    // Run invalid data reset test
    bool reset_pass = test_invalid_data_reset();
    if (reset_pass) passed_tests++;
    
    // Summary
    printf("=== Test Summary ===\n");
    printf("Convergence tests: %d/%d passed\n", passed_tests, num_tests);
    printf("Performance test: %s\n", performance_pass ? "PASS" : "FAIL");
    printf("Reset test: %s\n", reset_pass ? "PASS" : "FAIL");
    printf("Overall: %d/%d tests passed\n\n", passed_tests, num_tests + 2);
    
    return (passed_tests == num_tests + 2) ? 0 : 1;
}