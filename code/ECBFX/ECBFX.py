import numpy as np
from scipy.io import wavfile
from Crypto.Cipher import AES
import sys
import argparse
import sounddevice as sd

def generate_sine(amplitude, frequency, duration, sample_rate=44100):
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration))
    
    # Create sine wave
    sine = np.sin(2 * np.pi * frequency * t)

    sine *= amplitude
    
    # Convert to 16-bit integer
    sine_int16 = (sine * 32767).astype(np.int16)

    return sine_int16, sample_rate

def generate_triangle_wave(amplitude, frequency, duration, sample_rate=44100):
    # Calculate the number of samples
    num_samples = int(duration * sample_rate)
    
    # Generate time array
    t = np.linspace(0, duration, num_samples)
    
    # Generate triangle wave using arcsin of a sine wave
    triangle = (2 / np.pi) * np.arcsin(np.sin(2 * np.pi * frequency * t))
    
    # Scale the amplitude
    triangle = amplitude * triangle

    # Convert to 16-bit integer
    triangle_int16 = (triangle * 32767).astype(np.int16)
    
    return triangle_int16, sample_rate

def generate_square_wave(amplitude, frequency, duration, sample_rate=44100):
    # Generate time points
    t = np.linspace(0, duration, int(duration * sample_rate))
    
    # Generate square wave using sign of sine wave
    square = amplitude * np.sign(np.sin(2 * np.pi * frequency * t))

    # Convert to 16-bit integer
    square_int16 = (square * 32767).astype(np.int16)
    
    return square_int16, sample_rate

def generate_sawtooth(amplitude, frequency, duration, sample_rate=44100):
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration), False)
    
    # Generate sawtooth wave
    # The sawtooth function generates values from -1 to 1
    sawtooth = 2 * (frequency * t - np.floor(0.5 + frequency * t))
    
    # Scale the amplitude
    sawtooth = amplitude * sawtooth
    
    # Convert to 16-bit integer values
    sawtooth_int16 = (sawtooth * 32767).astype(np.int16)
    
    return sawtooth_int16, sample_rate

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Process and encrypt audio waveforms while preserving musical patterns.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    
    parser.add_argument(
        '-s', '--shape',
        choices=['sine', 'triangle', 'square', 'sawtooth'],
        required=True,
        help='Wave shape to generate'
    )

    parser.add_argument(
        '-a', '--amplitude',
        type=float,
        default=0.5,
        help='Amplitude of the wave (0.0 to 1.0)'
    )
    
    parser.add_argument(
        '-f', '--frequency',
        type=float,
        default=440.0,
        help='Frequency in Hz'
    )
    
    parser.add_argument(
        '-d', '--duration',
        type=float,
        default=3.0,
        help='Duration in seconds'
    )
    
    parser.add_argument(
        '-k', '--key',
        default='4321432143214321',
        help='16-byte encryption key'
    )

    parser.add_argument(
        '-w', '--wet',
        type=float,
        default=50,
        help='Wet/dry knob (0 = dry, 50 = 50/50 mix, 100 = maximum wet)'
    )
    
    parser.add_argument(
        '-o', '--output',
        help='Optional output WAV filename'
    )
    
    args = parser.parse_args()

    # Convert knob value to number between 0 and 1
    args.wet *= 0.01

    return args

def normalize_audio(encrypted_audio, original_audio):
    """Normalize encrypted audio to match the amplitude range of the original audio"""
    # Convert to float for calculations
    encrypted_float = encrypted_audio.astype(np.float32)
    original_float = original_audio.astype(np.float32)
    
    # Calculate RMS values
    if len(original_audio.shape) > 1:
        # Stereo
        original_rms = np.sqrt(np.mean(original_float**2, axis=0))
        encrypted_rms = np.sqrt(np.mean(encrypted_float**2, axis=0))
        
        # Calculate scaling factors for each channel
        scaling_factors = original_rms / encrypted_rms
        
        # Apply scaling to each channel
        normalized = encrypted_float * scaling_factors
    else:
        # Mono
        original_rms = np.sqrt(np.mean(original_float**2))
        encrypted_rms = np.sqrt(np.mean(encrypted_float**2))
        
        # Calculate and apply scaling factor
        scaling_factor = original_rms / encrypted_rms
        normalized = encrypted_float * scaling_factor
    
    # Clip to int16 range and convert back
    return np.clip(normalized, -32768, 32767).astype(np.int16)

def quantize_audio(audio_data):
    """Quantize audio into distinct levels to create more visible patterns"""
    # Fixed number of quantization levels
    LEVELS = 32
    
    # Convert to float between -1 and 1
    float_data = audio_data.astype(np.float32) / 32768.0
    
    # Quantize to specified number of levels
    quantized = np.round(float_data * (LEVELS/2)) * (2.0/LEVELS)
    
    # Convert back to int16
    return (quantized * 32768).astype(np.int16)

def reshape_for_encryption(audio_data):
    """Reshape audio into larger chunks for pattern preservation"""
    # Use fixed chunk size - 2048 samples per chunk (4096 bytes)
    CHUNK_SIZE = 2048
    
    # Handle both mono and stereo
    if len(audio_data.shape) == 1:
        # Mono audio
        num_chunks = len(audio_data) // CHUNK_SIZE
        trimmed_length = num_chunks * CHUNK_SIZE
        audio_trimmed = audio_data[:trimmed_length]
        chunks = audio_trimmed.reshape(-1, CHUNK_SIZE)
    else:
        # Stereo audio
        num_chunks = audio_data.shape[0] // CHUNK_SIZE
        trimmed_length = num_chunks * CHUNK_SIZE
        audio_trimmed = audio_data[:trimmed_length, :]
        # Reshape each channel separately
        left_chunks = audio_trimmed[:, 0].reshape(-1, CHUNK_SIZE)
        right_chunks = audio_trimmed[:, 1].reshape(-1, CHUNK_SIZE)
        chunks = (left_chunks, right_chunks)
    
    return chunks, trimmed_length

def encrypt_chunk(chunk, cipher):
    """Encrypt a single chunk of audio data"""
    # Ensure the chunk size is a multiple of 16 bytes
    orig_shape = chunk.shape
    chunk_bytes = chunk.tobytes()
    padding_size = (16 - (len(chunk_bytes) % 16)) % 16
    if padding_size:
        chunk_bytes += b'\0' * padding_size
    
    encrypted_bytes = cipher.encrypt(chunk_bytes)
    # Convert back to samples, preserving only the original shape
    decrypted_samples = np.frombuffer(encrypted_bytes, dtype=np.int16)
    return decrypted_samples[:orig_shape[0]]

def process_chunks(chunks, key):
    """Process all chunks while preserving structure"""
    cipher = AES.new(key.encode(), AES.MODE_ECB)
    
    if isinstance(chunks, tuple):
        # Stereo audio - process each channel
        left_encrypted = []
        right_encrypted = []
        for left_chunk, right_chunk in zip(chunks[0], chunks[1]):
            left_encrypted.append(encrypt_chunk(left_chunk, cipher))
            right_encrypted.append(encrypt_chunk(right_chunk, cipher))
        return (np.vstack(left_encrypted), np.vstack(right_encrypted))
    else:
        # Mono audio
        encrypted_chunks = []
        for chunk in chunks:
            encrypted_chunks.append(encrypt_chunk(chunk, cipher))
        return np.vstack(encrypted_chunks)
    
def wet_dry_mix(encrypted_data, input_data, knob):
    """Mix between encrypted (wet) and original (dry) signals
    knob: 0.0 = 100% dry, 1.0 = 100% wet"""
    if knob > 1:
        knob = 1
    out = (encrypted_data * knob) + (input_data * (1 - knob))
    return out

def main():
    # Parse command line arguments
    args = parse_arguments()
    
    # Generate waveform based on shape
    if args.shape == 'sine':
        audio_data, sample_rate = generate_sine(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
        )
    elif args.shape == 'triangle':
        audio_data, sample_rate = generate_triangle_wave(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
        )
    elif args.shape == 'square':
        audio_data, sample_rate = generate_square_wave(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
        )
    elif args.shape == 'sawtooth':
        audio_data, sample_rate = generate_sawtooth(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
        )

    # If wet is 0, just use the generated waveform
    if args.wet == 0:
        final_audio = audio_data
    else:
        # Validate key length
        if len(args.key) != 16:
            print("Error: Encryption key must be exactly 16 bytes")
            sys.exit(1)
        
        # Quantize the audio
        quantized_audio = quantize_audio(audio_data)
        
        # Reshape into chunks
        audio_chunks, trimmed_length = reshape_for_encryption(quantized_audio)
        
        # Encrypt chunks
        encrypted_chunks = process_chunks(audio_chunks, args.key)

        # Prepare final audio data
        encrypted_audio = encrypted_chunks.flatten()
        original_audio = audio_chunks.flatten()
        
        # Trim to original length if needed
        if len(encrypted_audio) > trimmed_length:
            encrypted_audio = encrypted_audio[:trimmed_length]
            original_audio = original_audio[:trimmed_length]
        
        # Now do the wet/dry mix with the properly shaped arrays
        final_audio = wet_dry_mix(encrypted_audio, original_audio, args.wet)
        
        # Normalize the final audio
        final_audio = normalize_audio(final_audio, quantized_audio)

    # If output file is specified, save to WAV
    if args.output:
        try:
            wavfile.write(args.output, sample_rate, final_audio)
            print(f"Saved audio to: {args.output}")
        except Exception as e:
            print(f"Error saving audio file: {e}")
            sys.exit(1)
    else:
        # Only play if no output file is specified
        try:
            print("Playing audio...")
            # Convert to float32 for sounddevice
            audio_float = final_audio.astype(np.float32) / 32768.0
            sd.play(audio_float, sample_rate)
            sd.wait()  # Wait until the audio is done playing
        except Exception as e:
            print(f"Error playing audio: {e}")

if __name__ == "__main__":
    main()