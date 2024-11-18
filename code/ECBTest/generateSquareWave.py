import numpy as np
import wave
import struct

def generate_square_wave(frequency=440.0, duration=1.0, amplitude=0.5, sample_rate=44100):
    """
    Generate a square wave signal.
    
    Parameters:
    frequency (float): Frequency of the square wave in Hz
    duration (float): Duration of the signal in seconds
    amplitude (float): Peak amplitude of the wave (0.0 to 1.0)
    sample_rate (int): Number of samples per second
    
    Returns:
    numpy.ndarray: Array of square wave values
    """
    # Generate time points
    t = np.linspace(0, duration, int(duration * sample_rate))
    
    # Generate square wave using sign of sine wave
    square = amplitude * np.sign(np.sin(2 * np.pi * frequency * t))
    
    return square

def save_wave_file(filename, samples, sample_rate=44100):
    """
    Save the wave data to a .wav file.
    
    Parameters:
    filename (str): Name of the output file
    samples (numpy.ndarray): Array of wave values
    sample_rate (int): Number of samples per second
    """
    # Convert floating point samples to 16-bit integers
    samples_int = (samples * 32767).astype(np.int16)
    
    # Open wave file
    with wave.open(filename, 'w') as wave_file:
        # Set parameters
        nchannels = 1  # Mono
        sampwidth = 2  # 2 bytes per sample (16-bit)
        
        # Set wave file parameters
        wave_file.setnchannels(nchannels)
        wave_file.setsampwidth(sampwidth)
        wave_file.setframerate(sample_rate)
        
        # Write data
        wave_file.writeframes(samples_int.tobytes())

# Generate a square wave (440 Hz = A4 note)
sample_rate = 44100  # CD quality audio
duration = 2.0  # seconds
frequency = 440.0  # Hz (A4 note)
amplitude = 0.5  # 50% amplitude to avoid clipping

wave_data = generate_square_wave(
    frequency=frequency,
    duration=duration,
    amplitude=amplitude,
    sample_rate=sample_rate
)

# Save to file
save_wave_file('square_wave.wav', wave_data, sample_rate)