import numpy as np
from scipy.io import wavfile

def generate_sawtooth(frequency=440, duration=3, sample_rate=44100, amplitude=0.5):
    """
    Generate a sawtooth wave.
    
    Parameters:
    frequency (float): Frequency of the wave in Hz
    duration (float): Duration of the wave in seconds
    sample_rate (int): Number of samples per second
    amplitude (float): Amplitude of the wave (0.0 to 1.0)
    """
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    
    # Generate sawtooth wave
    # The sawtooth function generates values from -1 to 1
    sawtooth = 2 * (frequency * t - np.floor(0.5 + frequency * t))
    
    # Scale the amplitude
    sawtooth = amplitude * sawtooth
    
    # Convert to 16-bit integer values
    audio_data = (sawtooth * 32767).astype(np.int16)
    
    return audio_data, sample_rate

def save_wave(filename, audio_data, sample_rate):
    """Save the audio data to a WAV file."""
    wavfile.write(filename, sample_rate, audio_data)

if __name__ == "__main__":
    # Generate a 440 Hz sawtooth wave
    audio_data, sample_rate = generate_sawtooth(
        frequency=440,    # A4 note
        duration=3,       # 3 seconds
        sample_rate=44100,# CD quality
        amplitude=0.5     # 50% amplitude
    )
    
    # Save to file
    save_wave("sawtooth.wav", audio_data, sample_rate)