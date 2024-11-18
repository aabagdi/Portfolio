import numpy as np
from scipy.io import wavfile

def generate_triangle_wave(frequency, duration, sample_rate=44100, amplitude=0.5):
    """
    Generate a triangle wave with specified parameters.
    
    Parameters:
    frequency (float): Frequency of the wave in Hz
    duration (float): Duration of the wave in seconds
    sample_rate (int): Number of samples per second
    amplitude (float): Amplitude of the wave (0.0 to 1.0)
    
    Returns:
    numpy.ndarray: The generated triangle wave
    """
    # Calculate the number of samples
    num_samples = int(duration * sample_rate)
    
    # Generate time array
    t = np.linspace(0, duration, num_samples)
    
    # Generate triangle wave using arcsin of a sine wave
    triangle = (2 / np.pi) * np.arcsin(np.sin(2 * np.pi * frequency * t))
    
    # Scale the amplitude
    triangle = amplitude * triangle
    
    return triangle

def save_wave_to_file(wave_data, filename, sample_rate=44100):
    """
    Save the wave data to a WAV file.
    
    Parameters:
    wave_data (numpy.ndarray): The wave data to save
    filename (str): Output filename
    sample_rate (int): Number of samples per second
    """
    # Scale to 16-bit integer values
    scaled = np.int16(wave_data * 32767)
    wavfile.write(filename, sample_rate, scaled)

# Example usage
if __name__ == "__main__":
    # Parameters
    freq = 440.0  # frequency in Hz (A4 note)
    duration = 2.0  # duration in seconds
    sample_rate = 44100  # CD quality audio
    amplitude = 0.5  # 50% amplitude
    
    # Generate the wave
    triangle_wave = generate_triangle_wave(freq, duration, sample_rate, amplitude)
    
    # Save to file
    save_wave_to_file(triangle_wave, "triangle_wave.wav", sample_rate)
    print(f"Generated triangle wave at {freq}Hz for {duration} seconds")