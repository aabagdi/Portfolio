import numpy as np
from scipy.io import wavfile

def generate_test_sine():
    # Parameters
    duration = 2  # seconds
    sample_rate = 44100
    frequency = 440  # Hz
    
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration))
    
    # Create sine wave
    sine = np.sin(2 * np.pi * frequency * t)
    
    # Convert to 16-bit integer
    sine_int16 = (sine * 32767).astype(np.int16)
    
    # Save the file
    wavfile.write('test_sine.wav', sample_rate, sine_int16)
    print("Created test_sine.wav")

if __name__ == "__main__":
    generate_test_sine()