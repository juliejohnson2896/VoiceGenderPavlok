#ifndef SIMPLE_RESAMPLER_H
#define SIMPLE_RESAMPLER_H

#include <vector>
#include <cmath>
#include <algorithm>

namespace Resampler {
// Performs linear interpolation to resample an audio signal.
//
// @param      input    The original audio signal.
// @param      in_rate  The sample rate of the input signal.
// @param      out_rate The desired output sample rate.
// @return     The resampled audio signal.
//
    inline std::vector<double> linear(const std::vector<double>& input, double in_rate, double out_rate) {
        if (input.empty() || in_rate <= 0 || out_rate <= 0) {
            return {};
        }

        if (in_rate == out_rate) {
            return input;
        }

        double ratio = in_rate / out_rate;
        size_t out_len = static_cast<size_t>(static_cast<double>(input.size()) / ratio);
        if (out_len == 0) {
            return {};
        }

        std::vector<double> output(out_len);

        for (size_t i = 0; i < out_len; ++i) {
            double in_index_float = static_cast<double>(i) * ratio;
            size_t index1 = static_cast<size_t>(in_index_float);

            // Ensure index2 is within bounds
            size_t index2 = std::min(index1 + 1, input.size() - 1);

            // Prevent reading past the end of the input buffer
            if (index1 >= input.size()) {
                index1 = input.size() - 1;
            }

            double fraction = in_index_float - static_cast<double>(index1);

            output[i] = input[index1] * (1.0 - fraction) + input[index2] * fraction;
        }

        return output;
    }

} // namespace Resampler

#endif // SIMPLE_RESAMPLER_H