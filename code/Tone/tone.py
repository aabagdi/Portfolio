import argparse
import numpy as np
from scipy import io, signal   
import sounddevice as sd

# Size of output buffer in frames. Less than 1024 is not
# recommended, as most audio interfaces will choke
# horribly.
BUFFER_SIZE = 2048

# Read from a 16-bit WAV file. Returns the sample rate in
# samples per second, and the samples as a numpy array of
# IEEE 64-bit floats. The array will be 1D for mono data,
# and will be a 2D array of 2-element frames for stereo
# data.
def read_wav(filename):
    rate, data = io.wavfile.read(filename)
    assert data.dtype == np.int16
    data = data.astype(np.float64)
    data /= 32768
    return rate, data

# Write to a 16-bit WAV file. Data is in the same
# format produced by read_wav().
def write_wav(filename, rate, data):
    assert data.dtype == np.float64
    data *= 32767
    data = data.astype(np.int16)
    io.wavfile.write(filename, rate, data)

# Play a tone on the computer. Data is in the same format
# produced by read_wav().
def play(rate, wav):
    # Deal with stereo.
    channels = 1
    if wav.ndim == 2:
        channels = 2

    # Set up and start the stream.
    stream = sd.RawOutputStream(
        samplerate = rate,
        blocksize = BUFFER_SIZE,
        channels = channels,
        dtype = 'float32',
    )

    # Write the samples.
    stream.start()
    # https://stackoverflow.com/a/73368196
    indices = np.arange(BUFFER_SIZE, wav.shape[0], BUFFER_SIZE)
    samples = np.ascontiguousarray(wav, dtype=np.float32)
    for buffer in np.array_split(samples, indices):
        stream.write(buffer)

    # Tear down the stream.
    stream.stop()
    stream.close()

# Parse command-line arguments. Returns a struct whose
# elements are the arguments passed on the command line.
# See the `argparse` documentation for details.
def tone_args():
    argp = argparse.ArgumentParser()
    argp.add_argument(
        "--volume",
        help="volume in 3dB units (default 9 = 0dB, 0 = 0 output)",
        type=np.float64,
        default=9,
    )
    argp.add_argument(
        "--bass",
        help="bass emphasis in 3dB units (default 5 = 0dB, 0 = 0 output)",
        type=np.float64,
        default=5,
    )
    argp.add_argument(
        "--midrange",
        help="midrange emphasis in 3dB units (default 5 = 0dB, 0 = 0 output)",
        type=np.float64,
        default=5,
    )
    argp.add_argument(
        "--treble",
        help="treble emphasis in 3dB units (default 5 = 0dB, 0 = 0 output)",
        type=np.float64,
        default=5,
    )
    argp.add_argument(
        "--out",
        help="write to WAV file instead of playing",
    )
    argp.add_argument("wav", help="input audio file")
    return argp.parse_args()

def create_filters(rate):
    """
    Create a set of IIR filters for bass, mid, and treble bands.
    Returns filter coefficients (b, a) for each band.
    """
    # Crossover frequencies
    bass_limit = 300    # Hz
    mid_limit = 4000    # Hz
    
    # Normalize frequencies to Nyquist rate and ensure they're within valid range
    nyquist = rate / 2
    bass_norm = min(0.99 * bass_limit / nyquist, 0.99)  # Ensure < 1
    mid_norm = min(0.99 * mid_limit / nyquist, 0.99)    # Ensure < 1
    
    # Safety check - ensure frequencies are properly ordered and in valid range
    bass_norm = max(0.001, min(bass_norm, 0.99))  # Ensure > 0 and < 1
    mid_norm = max(bass_norm + 0.001, min(mid_norm, 0.99))  # Ensure > bass_norm and < 1
        
    # Create filters (4th order Butterworth)
    # Low band (bass): lowpass at bass_limit
    bass_b, bass_a = signal.butter(4, bass_norm, btype='lowpass')
    
    # Mid band: bandpass between bass_limit and mid_limit
    mid_b, mid_a = signal.butter(4, [bass_norm, mid_norm], btype='bandpass')
    
    # High band (treble): highpass at mid_limit
    treble_b, treble_a = signal.butter(4, mid_norm, btype='highpass')
    
    return (bass_b, bass_a), (mid_b, mid_a), (treble_b, treble_a)

def apply_tone_control(data, rate, bass_gain, mid_gain, treble_gain, volume):
    """
    Apply tone control to audio data using IIR filters.
    
    Parameters:
    - data: Input audio samples (mono or stereo)
    - rate: Sample rate in Hz
    - bass_gain: Bass emphasis (0-10 range, 5 is neutral)
    - mid_gain: Midrange emphasis (0-10 range, 5 is neutral)
    - treble_gain: Treble emphasis (0-10 range, 5 is neutral)
    - volume: Overall volume (0-10 range, 9 is neutral)
    
    Returns:
    - Processed audio data
    """
    # Handle stereo
    if data.ndim == 2:
        left = apply_tone_control(data[:, 0], rate, bass_gain, mid_gain, treble_gain, volume)
        right = apply_tone_control(data[:, 1], rate, bass_gain, mid_gain, treble_gain, volume)
        return np.column_stack((left, right))
    
    # Convert gains from 0-10 range to linear scale
    def scale_gain(gain, neutral=5):
        if gain < 0.1:
            return 0
        db = (gain - neutral) * 3  # 3dB units as specified
        return 10 ** (db / 20)
    
    try:
        # Get filter coefficients
        (bass_b, bass_a), (mid_b, mid_a), (treble_b, treble_a) = create_filters(rate)
        
        # Apply filters and gains to each band
        bass = signal.filtfilt(bass_b, bass_a, data) * scale_gain(bass_gain)
        mid = signal.filtfilt(mid_b, mid_a, data) * scale_gain(mid_gain)
        treble = signal.filtfilt(treble_b, treble_a, data) * scale_gain(treble_gain)
        
        # Sum the bands
        processed = bass + mid + treble
        
    except Exception as e:
        print(f"Warning: Filter processing failed ({str(e)}). Falling back to unprocessed audio.")
        processed = data
    
    # Apply overall volume
    processed *= scale_gain(volume, neutral=9)
    
    # Clip to prevent distortion
    processed = np.clip(processed, -1.0, 1.0)
    
    return processed

def main():
    args = tone_args()
    rate, data = read_wav(args.wav)
    
    # Process the audio
    processed = apply_tone_control(
        data, 
        rate,
        args.bass,
        args.midrange,
        args.treble,
        args.volume
    )
    
    # Output
    if args.out:
        write_wav(args.out, rate, processed)
    else:
        play(rate, processed)

if __name__ == "__main__":
    main()