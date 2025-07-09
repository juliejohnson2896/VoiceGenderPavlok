import numpy as np
import librosa
import json
import time
import os
import soundfile as sf
import sys

def log_message(tag, message):
    """A helper function for consistent logging."""
    print(f"[Chaquopy - {tag}] {message}")

def save_debug_audio(audio_data, sample_rate, app_context, filename_prefix="debug_"):
    """Saves the provided numpy audio buffer to a WAV file."""
    tag = "AudioSaver"
    try:
        cache_dir = app_context.getCacheDir().getAbsolutePath()
        timestamp = int(time.time() * 1000)
        filename = f"{filename_prefix}{timestamp}.wav"
        filepath = os.path.join(cache_dir, filename)
        sf.write(filepath, audio_data, sample_rate)
        log_message(tag, f"Successfully saved debug audio to: {filepath}")
    except Exception as e:
        log_message(tag, f"ERROR saving debug audio: {str(e)}")


# ==============================================================================
# MAIN ANALYSIS FUNCTION (With corrected noise reduction logic)
# ==============================================================================

def analyze_voice_features(audio_byte_buffer, sample_rate=44100, app_context=None, save_audio=False, byte_order='little'):
    tag = "MainAnalyzer"
#     log_message(tag, "--- New analysis request received ---")

    try:
        dt = np.dtype(np.float32).newbyteorder('<' if byte_order.lower() == 'little' else '>')
        audio_data = np.frombuffer(audio_byte_buffer, dtype=dt)
#         log_message(tag, f"Successfully created numpy array from buffer. Shape: {audio_data.shape}")

        # Pre-emphasize the audio to enhance lower frequencies
        audio_data = np.append(audio_data[0], audio_data[1:] - 0.97 * audio_data[:-1])

        # Apply gentle high-pass filter to remove DC offset
        from scipy.signal import butter, filtfilt
        b, a = butter(3, 80.0 / (sample_rate/2), btype='high')
        audio_data = filtfilt(b, a, audio_data)

        # Skip noise reduction - use original audio directly
        pitch_stats = get_pitch_statistics(audio_data, sample_rate)
        formants = analyze_formants(audio_data, sample_rate)
        hnr = analyze_hnr(audio_data, sample_rate)

        log_message(tag, f"Audio Pitch Stats: {pitch_stats.get('mean_pitch', 0.0)}")

        results = {
            'pitch_hz': pitch_stats.get('mean_pitch', 0.0),
            'formants': formants,
            'hnr_db': hnr
        }

        return json.dumps(results)
    except Exception as e:
        log_message(tag, f"CRITICAL ERROR in main function: {str(e)}")
        return json.dumps({'error': str(e)})

# --- Full definitions of helper functions for completeness ---
def get_pitch_statistics(audio_data, sample_rate=44100):
    """
    Universal pitch detection for both male and female voices.
    Uses multiple methods and octave error correction.
    """
    tag = "PitchStats"
    try:
        # Method 1: Wide-range YIN for initial estimate
        pitches_yin = librosa.yin(y=audio_data, fmin=50, fmax=500, sr=sample_rate)

        # Method 2: Piptrack for comparison
        pitches_pip, magnitudes = librosa.piptrack(y=audio_data, sr=sample_rate,
                                                  fmin=50, fmax=500, threshold=0.1)

        # Method 3: Autocorrelation-based (backup method)
        def autocorr_pitch(audio, sr):
            # Simple autocorrelation pitch detection
            autocorr = np.correlate(audio, audio, mode='full')
            autocorr = autocorr[len(autocorr)//2:]

            # Find peak in reasonable pitch range
            min_period = int(sr / 500)  # 500 Hz max
            max_period = int(sr / 50)   # 50 Hz min

            if max_period < len(autocorr):
                peak_idx = np.argmax(autocorr[min_period:max_period]) + min_period
                return sr / peak_idx
            return 0

        # Filter YIN results
        voiced_pitches_yin = [p for p in pitches_yin if p > 0 and not np.isnan(p)]

        # Extract piptrack results
        voiced_pitches_pip = []
        for t in range(pitches_pip.shape[1]):
            pitch_col = pitches_pip[:, t]
            mag_col = magnitudes[:, t]
            if len(pitch_col[pitch_col > 0]) > 0:
                # Get the pitch with highest magnitude
                max_mag_idx = np.argmax(mag_col)
                if pitch_col[max_mag_idx] > 0:
                    voiced_pitches_pip.append(pitch_col[max_mag_idx])

        # Get autocorrelation estimate
        autocorr_pitch_val = autocorr_pitch(audio_data, sample_rate)

        log_message(tag, f"YIN pitches count: {len(voiced_pitches_yin)}")
        log_message(tag, f"Piptrack pitches count: {len(voiced_pitches_pip)}")
        log_message(tag, f"Autocorr pitch: {autocorr_pitch_val:.1f} Hz")

        if not voiced_pitches_yin and not voiced_pitches_pip and autocorr_pitch_val == 0:
            return {'mean_pitch': 0.0}

        # Combine all estimates
        all_estimates = []

        if voiced_pitches_yin:
            yin_mean = np.mean(voiced_pitches_yin)
            all_estimates.append(yin_mean)
            log_message(tag, f"YIN mean: {yin_mean:.1f} Hz")

        if voiced_pitches_pip:
            pip_mean = np.mean(voiced_pitches_pip)
            all_estimates.append(pip_mean)
            log_message(tag, f"Piptrack mean: {pip_mean:.1f} Hz")

        if autocorr_pitch_val > 0:
            all_estimates.append(autocorr_pitch_val)

        if not all_estimates:
            return {'mean_pitch': 0.0}

        # Smart octave error correction
        def correct_octave_errors(estimates):
            corrected = []
            for est in estimates:
                # Test both the original and half/double frequencies
                candidates = [est, est/2, est*2]

                # Score each candidate based on how "reasonable" it is
                scores = []
                for candidate in candidates:
                    if 80 <= candidate <= 180:      # Male range
                        scores.append(3)
                    elif 150 <= candidate <= 300:   # Female range
                        scores.append(2)
                    elif 50 <= candidate <= 400:    # Extended human range
                        scores.append(1)
                    else:
                        scores.append(0)

                # Pick the candidate with the highest score
                best_idx = np.argmax(scores)
                corrected.append(candidates[best_idx])

            return corrected

        corrected_estimates = correct_octave_errors(all_estimates)

        # Final pitch: median of corrected estimates (more robust than mean)
        final_pitch = np.median(corrected_estimates)

        log_message(tag, f"Original estimates: {[f'{e:.1f}' for e in all_estimates]}")
        log_message(tag, f"Corrected estimates: {[f'{e:.1f}' for e in corrected_estimates]}")
        log_message(tag, f"Final pitch: {final_pitch:.1f} Hz")

        return {'mean_pitch': float(final_pitch)}

    except Exception as e:
        log_message(tag, f"ERROR: {str(e)}")
        return {'error': str(e), 'mean_pitch': 0.0}

def analyze_formants(audio_data, sample_rate=44100, num_formants=4):
    tag = "Formants"
    try:
        windowed_data = audio_data * np.hanning(len(audio_data))
        order = int(sample_rate / 1000) + 2
        a = librosa.lpc(windowed_data, order=order)
        roots = np.roots(a)
        roots = [r for r in roots if np.imag(r) >= 0]
        angles = np.angle(roots)
        freqs = sorted(angles * (sample_rate / (2 * np.pi)))
        return [f for f in freqs if 300 < f < 4000][:num_formants]
    except Exception as e:
        log_message(tag, f"ERROR: {str(e)}")
        return []

def analyze_hnr(audio_data, sample_rate=44100):
    tag = "HNR"
    try:
        y_harmonic, y_percussive = librosa.effects.hpss(audio_data)
        rms_harmonic = np.mean(librosa.feature.rms(y=y_harmonic))
        rms_percussive = np.mean(librosa.feature.rms(y=y_percussive))
        if rms_percussive == 0: return 100.0
        if rms_harmonic == 0: return 0.0
        return float(20 * np.log10(rms_harmonic / rms_percussive))
    except Exception as e:
        log_message(tag, f"ERROR: {str(e)}")
        return 0.0