import numpy as np
import scipy.signal
from scipy.fft import rfft, rfftfreq

class FastPitchAnalyzer:
    """
    Ultra-fast pitch analyzer optimized for real-time Android use.
    Uses simple but effective algorithms for speed.
    """

    def __init__(self, sample_rate=22050, frame_size=1024):
        """
        Initialize the fast pitch analyzer.

        Args:
            sample_rate: Sample rate of the audio data
            frame_size: Size of analysis frame (smaller = faster)
        """
        self.sample_rate = sample_rate
        self.frame_size = frame_size
        self.min_freq = 65.0   # C2 - lowest expected voice
        self.max_freq = 1000.0 # Practical upper limit for speed

        # Pre-compute values for speed
        self.min_period = int(sample_rate / self.max_freq)
        self.max_period = int(sample_rate / self.min_freq)

        # Pre-allocate arrays
        self.autocorr_buffer = np.zeros(frame_size)
        self.window = np.hanning(frame_size)

        # Frequency bins for FFT method
        self.fft_freqs = rfftfreq(frame_size, 1/sample_rate)
        self.freq_mask = (self.fft_freqs >= self.min_freq) & (self.fft_freqs <= self.max_freq)

    def get_pitch_autocorr(self, audio_buffer):
        """
        Fast autocorrelation-based pitch detection.

        Args:
            audio_buffer: Audio samples as numpy array or list

        Returns:
            float: Fundamental frequency in Hz (0 if no pitch)
        """
        # Convert and validate input
        if not isinstance(audio_buffer, np.ndarray):
            audio_buffer = np.array(audio_buffer, dtype=np.float32)

        if len(audio_buffer) < self.frame_size:
            return 0.0

        # Take only the frame we need
        frame = audio_buffer[:self.frame_size]

        # Apply window and remove DC
        frame = (frame - np.mean(frame)) * self.window

        # Fast autocorrelation using FFT
        fft_data = rfft(frame)
        autocorr = np.real(np.fft.irfft(fft_data * np.conj(fft_data)))

        # Find peak in valid period range
        autocorr = autocorr[self.min_period:self.max_period]

        if len(autocorr) == 0:
            return 0.0

        # Find the highest peak
        peak_idx = np.argmax(autocorr)
        period = peak_idx + self.min_period

        # Confidence check - peak should be significant
        if autocorr[peak_idx] < 0.3 * autocorr[0]:
            return 0.0

        return self.sample_rate / period

    def get_pitch_fft(self, audio_buffer):
        """
        Fast FFT-based pitch detection using harmonic product spectrum.

        Args:
            audio_buffer: Audio samples as numpy array or list

        Returns:
            float: Fundamental frequency in Hz (0 if no pitch)
        """
        # Convert and validate input
        if not isinstance(audio_buffer, np.ndarray):
            audio_buffer = np.array(audio_buffer, dtype=np.float32)

        if len(audio_buffer) < self.frame_size:
            return 0.0

        # Take only the frame we need
        frame = audio_buffer[:self.frame_size]

        # Apply window and remove DC
        frame = (frame - np.mean(frame)) * self.window

        # FFT
        fft_data = rfft(frame)
        magnitude = np.abs(fft_data)

        # Focus on voice frequency range
        magnitude = magnitude[self.freq_mask]
        freqs = self.fft_freqs[self.freq_mask]

        if len(magnitude) == 0:
            return 0.0

        # Simple harmonic product spectrum (2 harmonics for speed)
        hps = magnitude.copy()
        if len(hps) > 2:
            # Downsample and multiply (harmonic product)
            hps2 = np.interp(freqs, freqs[::2], magnitude[::2])
            hps *= hps2

        # Find peak
        peak_idx = np.argmax(hps)

        # Confidence check
        if hps[peak_idx] < 0.1 * np.max(magnitude):
            return 0.0

        return freqs[peak_idx]

    def get_pitch_zero_crossing(self, audio_buffer):
        """
        Ultra-fast zero-crossing rate based pitch estimation.
        Less accurate but very fast.

        Args:
            audio_buffer: Audio samples as numpy array or list

        Returns:
            float: Estimated fundamental frequency in Hz (0 if no pitch)
        """
        # Convert and validate input
        if not isinstance(audio_buffer, np.ndarray):
            audio_buffer = np.array(audio_buffer, dtype=np.float32)

        if len(audio_buffer) < 100:
            return 0.0

        # Take smaller chunk for speed
        chunk_size = min(512, len(audio_buffer))
        frame = audio_buffer[:chunk_size]

        # Remove DC and check for voice activity
        frame = frame - np.mean(frame)
        if np.std(frame) < 0.01:  # Too quiet
            return 0.0

        # Count zero crossings
        zero_crossings = np.sum(np.diff(np.signbit(frame)))

        # Estimate frequency (rough approximation)
        estimated_freq = (zero_crossings * self.sample_rate) / (2 * len(frame))

        # Validate range
        if estimated_freq < self.min_freq or estimated_freq > self.max_freq:
            return 0.0

        return estimated_freq

    def get_pitch_fast(self, audio_buffer, method='autocorr'):
        """
        Get pitch using the specified fast method.

        Args:
            audio_buffer: Audio samples
            method: 'autocorr', 'fft', or 'zero_crossing'

        Returns:
            float: Fundamental frequency in Hz (0 if no pitch)
        """
        if method == 'autocorr':
            return self.get_pitch_autocorr(audio_buffer)
        elif method == 'fft':
            return self.get_pitch_fft(audio_buffer)
        elif method == 'zero_crossing':
            return self.get_pitch_zero_crossing(audio_buffer)
        else:
            return self.get_pitch_autocorr(audio_buffer)  # Default

    def get_pitch_with_confidence(self, audio_buffer):
        """
        Get pitch with a simple confidence measure.

        Args:
            audio_buffer: Audio samples

        Returns:
            dict: {'pitch_hz': float, 'confidence': float, 'is_voiced': bool}
        """
        # Use autocorr method with confidence estimation
        if not isinstance(audio_buffer, np.ndarray):
            audio_buffer = np.array(audio_buffer, dtype=np.float32)

        if len(audio_buffer) < self.frame_size:
            return {'pitch_hz': 0.0, 'confidence': 0.0, 'is_voiced': False}

        frame = audio_buffer[:self.frame_size]
        frame = (frame - np.mean(frame)) * self.window

        # Quick energy check
        energy = np.mean(frame**2)
        if energy < 1e-6:
            return {'pitch_hz': 0.0, 'confidence': 0.0, 'is_voiced': False}

        # Autocorrelation
        fft_data = rfft(frame)
        autocorr = np.real(np.fft.irfft(fft_data * np.conj(fft_data)))
        autocorr = autocorr[self.min_period:self.max_period]

        if len(autocorr) == 0:
            return {'pitch_hz': 0.0, 'confidence': 0.0, 'is_voiced': False}

        peak_idx = np.argmax(autocorr)
        period = peak_idx + self.min_period
        pitch_hz = self.sample_rate / period

        # Simple confidence based on peak height
        confidence = autocorr[peak_idx] / autocorr[0] if autocorr[0] > 0 else 0.0
        confidence = min(confidence, 1.0)

        is_voiced = confidence > 0.3

        return {
            'pitch_hz': float(pitch_hz),
            'confidence': float(confidence),
            'is_voiced': bool(is_voiced)
        }

# Global analyzer instance
_fast_analyzer = None

def init_fast_analyzer(sample_rate=22050, frame_size=1024):
    """
    Initialize the fast analyzer. Call once at startup.

    Args:
        sample_rate: Sample rate of your audio
        frame_size: Analysis frame size (smaller = faster, larger = more accurate)
    """
    global _fast_analyzer
    _fast_analyzer = FastPitchAnalyzer(sample_rate, frame_size)

def get_pitch_realtime(audio_buffer, method='autocorr'):
    """
    Get pitch in real-time using fast methods.

    Args:
        audio_buffer: Audio samples
        method: 'autocorr' (best), 'fft' (good), 'zero_crossing' (fastest)

    Returns:
        float: Frequency in Hz (0 if no pitch)
    """
    global _fast_analyzer
    if _fast_analyzer is None:
        init_fast_analyzer()

    return _fast_analyzer.get_pitch_fast(audio_buffer, method)

def get_pitch_simple(audio_buffer):
    """
    Simplest and fastest pitch detection.

    Args:
        audio_buffer: Audio samples

    Returns:
        float: Frequency in Hz (0 if no pitch)
    """
    return get_pitch_realtime(audio_buffer, 'zero_crossing')

def get_pitch_accurate(audio_buffer):
    """
    More accurate but still fast pitch detection.

    Args:
        audio_buffer: Audio samples

    Returns:
        float: Frequency in Hz (0 if no pitch)
    """
    return get_pitch_realtime(audio_buffer, 'autocorr')

def analyze_pitch_fast(audio_buffer):
    """
    Fast pitch analysis with confidence.

    Args:
        audio_buffer: Audio samples

    Returns:
        dict: {'pitch_hz': float, 'confidence': float, 'is_voiced': bool}
    """
    global _fast_analyzer
    if _fast_analyzer is None:
        init_fast_analyzer()

    return _fast_analyzer.get_pitch_with_confidence(audio_buffer)

def is_voice_present(audio_buffer, energy_threshold=0.001):
    """
    Ultra-fast voice activity detection.

    Args:
        audio_buffer: Audio samples
        energy_threshold: Minimum energy threshold

    Returns:
        bool: True if voice is likely present
    """
    if not isinstance(audio_buffer, np.ndarray):
        audio_buffer = np.array(audio_buffer, dtype=np.float32)

    if len(audio_buffer) < 100:
        return False

    # Quick energy check
    energy = np.mean(audio_buffer**2)
    if energy < energy_threshold:
        return False

    # Quick zero-crossing rate check
    zero_crossings = np.sum(np.diff(np.signbit(audio_buffer)))
    zcr = zero_crossings / len(audio_buffer)

    # Voice typically has moderate zero-crossing rate
    return 0.01 < zcr < 0.3

# Batch processing for multiple frames
def process_audio_stream(audio_chunks, sample_rate=22050):
    """
    Process multiple audio chunks efficiently.

    Args:
        audio_chunks: List of audio buffer chunks
        sample_rate: Sample rate

    Returns:
        list: List of pitch values for each chunk
    """
    init_fast_analyzer(sample_rate)

    results = []
    for chunk in audio_chunks:
        pitch = get_pitch_realtime(chunk, 'autocorr')
        results.append(pitch)

    return results