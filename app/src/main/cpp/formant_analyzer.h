#ifndef FORMANT_ANALYZER_H
#define FORMANT_ANALYZER_H

#include <vector>
#include <string>

// Struct to hold the result for a single formant.
// This structure will be used to return the frequency and bandwidth
// of each detected formant.
struct Formant {
    double frequency_hz;
    double bandwidth_hz;
};

// Main class to perform Praat-like formant analysis.
// This class encapsulates all the stages of the pipeline:
// resampling, framing, pre-processing, LPC analysis, and formant extraction.
class formant_analyzer {
public:
    // The main public static method that processes an entire audio buffer.
    // It returns a vector of formant lists, one list for each analysis frame.
    static std::vector<std::vector<Formant>> process(
            const std::vector<double>& audio_data,
            double original_sample_rate,
            double formant_ceiling_hz,
            int num_formants,
            double window_length_s,
            double pre_emphasis_from_hz
    );

private:
    // --- Internal helper methods mirroring the pipeline stages ---

    // Resamples the audio signal to the target rate defined by the formant ceiling.
    static std::vector<double> resample(const std::vector<double>& input, double in_rate, double out_rate);

    // Prepares a single frame by applying a window function and pre-emphasis filter.
    static std::vector<double> preprocess_frame(const std::vector<double>& audio, size_t center_sample, double window_len_s, double pre_emph_hz, double sample_rate);

    // Computes LPC coefficients for a given frame using the Burg algorithm.
    static std::vector<double> burg_lpc(const std::vector<double>& frame, int order);

    // Converts LPC coefficients into a final list of formants, including root-finding and filtering.
    static std::vector<Formant> lpc_to_formants(const std::vector<double>& lpc_coeffs, double formant_ceiling_hz, int num_formants);
};

#endif // FORMANT_ANALYZER_H
