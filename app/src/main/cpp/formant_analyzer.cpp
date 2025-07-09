#include "formant_analyzer.h"
#include "find_polynomial_roots_jenkins_traub.h" // <-- Use the new root-finder
#include "simple_resampler.h" // Assuming this file exists from previous steps
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <complex>
#include <Eigen/Dense> // Required for RpolyPlusPlus

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Main processing function remains the same
std::vector<std::vector<Formant>> formant_analyzer::process(
        const std::vector<double>& audio_data, double original_sample_rate,
        double formant_ceiling_hz, int num_formants, double window_length_s,
        double pre_emphasis_from_hz)
{
    if (audio_data.empty()) {
        return {};
    }

    double target_sample_rate = 2.0 * formant_ceiling_hz;
    std::vector<double> resampled_audio = resample(audio_data, original_sample_rate, target_sample_rate);

    // Set the time step to be 1/4 of the window length for high-resolution tracking.
    double actual_time_step_s = window_length_s / 4.0;

    size_t time_step_samples = static_cast<size_t>(actual_time_step_s * target_sample_rate);
    if (time_step_samples == 0) time_step_samples = 1; // Prevent infinite loop

    std::vector<std::vector<Formant>> all_frames_formants;
    size_t window_samples = static_cast<size_t>((window_length_s) * target_sample_rate);
    size_t half_window = window_samples / 2;

    for (size_t center = half_window; (center + half_window) < resampled_audio.size(); center += time_step_samples) {
        std::vector<double> processed_frame = preprocess_frame(
                resampled_audio, center, window_length_s, pre_emphasis_from_hz, target_sample_rate
        );

        int lpc_order = 2 * num_formants + 2; // Praat uses 2*num_formants + 2
        std::vector<double> lpc_coeffs = burg_lpc(processed_frame, lpc_order);
        if (lpc_coeffs.empty()) continue;

        std::vector<Formant> formants = lpc_to_formants(lpc_coeffs, formant_ceiling_hz, num_formants);
        all_frames_formants.push_back(formants);
    }

    return all_frames_formants;
}

// Resampling implementation (using SimpleResampler.h)
std::vector<double> formant_analyzer::resample(const std::vector<double>& input, double in_rate, double out_rate) {
    return Resampler::linear(input, in_rate, out_rate);
}

// Pre-processing implementation remains the same
std::vector<double> formant_analyzer::preprocess_frame(
        const std::vector<double>& audio, size_t center_sample, double window_len_s,
        double pre_emph_hz, double sample_rate)
{
    int window_size_samples = static_cast<int>(window_len_s * sample_rate);
    int half_window_size = window_size_samples / 2;

    size_t start_sample = (center_sample > (size_t)half_window_size) ? (center_sample - half_window_size) : 0;

    std::vector<double> frame(window_size_samples);

    // Gaussian window
    for (int i = 0; i < window_size_samples; ++i) {
        double x = (static_cast<double>(i) - half_window_size) / half_window_size;
        double window_val = std::exp(-12.5 * x * x); // Praat's Gaussian formula
        frame[i] = audio[start_sample + i] * window_val;
    }

    // Pre-emphasis
    double a = std::exp(-2.0 * M_PI * pre_emph_hz / sample_rate);
    for (size_t i = frame.size() - 1; i > 0; --i) {
        frame[i] -= a * frame[i - 1];
    }
    if (!frame.empty()) {
        frame[0] *= (1.0 - a);
    }

    return frame;
}

// Burg's method implementation remains the same
std::vector<double> formant_analyzer::burg_lpc(const std::vector<double>& frame, int order) {
    int N = frame.size();
    if (order <= 0 || order >= N) return {};

    std::vector<double> a(order + 1, 0.0);
    std::vector<double> k(order + 1, 0.0);
    std::vector<double> fwd_error = frame;
    std::vector<double> bwd_error = frame;

    a[0] = 1.0;

    for (int i = 1; i <= order; ++i) {
        double num = 0.0;
        double den = 0.0;
        for (int n = i; n < N; ++n) {
            num += fwd_error[n] * bwd_error[n - 1];
            den += fwd_error[n] * fwd_error[n] + bwd_error[n - 1] * bwd_error[n - 1];
        }

        if (den == 0.0) {
            k[i] = 0.0;
        } else {
            k[i] = -2.0 * num / den;
        }

        std::vector<double> a_temp = a;
        for (int j = 1; j <= i; ++j) {
            a[j] = a_temp[j] + k[i] * a_temp[i - j];
        }

        std::vector<double> prev_fwd_error = fwd_error;
        for (int n = i; n < N; ++n) {
            fwd_error[n] = prev_fwd_error[n] + k[i] * bwd_error[n - 1];
            bwd_error[n] = bwd_error[n - 1] + k[i] * prev_fwd_error[n];
        }
    }

    return a;
}

// *** UPDATED TO USE RpolyPlusPlus with Eigen vectors ***
std::vector<Formant> formant_analyzer::lpc_to_formants(const std::vector<double>& lpc_coeffs, double formant_ceiling_hz, int num_formants) {
    double sampling_rate_hz = 2.0 * formant_ceiling_hz;

    int degree = lpc_coeffs.size() - 1;
    if (degree <= 0) {
        return {};
    }

    // Convert std::vector to Eigen::VectorXd for RpolyPlusPlus
    Eigen::VectorXd poly_coeffs(lpc_coeffs.size());
    for (size_t i = 0; i < lpc_coeffs.size(); ++i) {
        poly_coeffs[i] = lpc_coeffs[i];
    }

    // Create output vectors for the roots
    Eigen::VectorXd real_parts(degree);
    Eigen::VectorXd imag_parts(degree);

    // Find the roots using RpolyPlusPlus
    bool success = rpoly_plus_plus::FindPolynomialRootsJenkinsTraub(poly_coeffs, &real_parts, &imag_parts);

    if (!success) {
        return {}; // Return empty vector if root finding failed
    }

    std::vector<Formant> candidates;
    for (int i = 0; i < degree; ++i) {
        // We only care about roots with a positive imaginary part
        if (imag_parts[i] > 1e-5) {
            std::complex<double> r(real_parts[i], imag_parts[i]);
            double angle = std::arg(r);
            double magnitude = std::abs(r);

            Formant f;
            f.frequency_hz = angle * (sampling_rate_hz / (2.0 * M_PI));
            f.bandwidth_hz = -std::log(magnitude) * (sampling_rate_hz / M_PI);

            // Filter for typical formant frequencies and bandwidths
            // A much more robust check that ignores F0
            if (f.frequency_hz > 200.0 && f.frequency_hz < (formant_ceiling_hz - 50.0) && f.bandwidth_hz < 800.0) {
                candidates.push_back(f);
            }
        }
    }

    // Sort formants by frequency
    std::sort(candidates.begin(), candidates.end(),
              [](const Formant& a, const Formant& b) {
                  return a.frequency_hz < b.frequency_hz;
              });

    // Return only the requested number of formants
    if (candidates.size() > static_cast<size_t>(num_formants)) {
        candidates.resize(num_formants);
    }

    return candidates;
}