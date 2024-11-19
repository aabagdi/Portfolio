import numpy as np
from scipy.io import wavfile
from Crypto.Cipher import AES
import sys
import argparse
import sounddevice as sd

def generate_sine(amplitude, frequency, duration, sample_rate):
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration))
    
    # Create sine wave
    sine = np.sin(2 * np.pi * frequency * t)

    sine *= amplitude
    
    # Convert to 16-bit integer
    sine_int16 = (sine * 32767).astype(np.int16)

    return sine_int16, sample_rate

def generate_triangle_wave(amplitude, frequency, duration, sample_rate):
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

def generate_square_wave(amplitude, frequency, duration, sample_rate):
    # Generate time points
    t = np.linspace(0, duration, int(duration * sample_rate))
    
    # Generate square wave using sign of sine wave
    square = amplitude * np.sign(np.sin(2 * np.pi * frequency * t))

    # Convert to 16-bit integer
    square_int16 = (square * 32767).astype(np.int16)
    
    return square_int16, sample_rate

def generate_sawtooth(amplitude, frequency, duration, sample_rate):
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

def generate_trapezoid_wave(amplitude, frequency, duration, sample_rate, duty_cycle=0.4):
    # Generate time array
    t = np.linspace(0, duration, int(sample_rate * duration))
    
    # Calculate the period
    period = 1.0 / frequency
    
    # Create trapezoid wave
    phase = (t % period) / period
    rise_time = 0.2  # Rise time as fraction of period
    fall_time = 0.2  # Fall time as fraction of period
    
    trapezoid = np.zeros_like(t)
    
    # Rising edge (from -1 to 1)
    mask_rise = (phase < rise_time)
    trapezoid[mask_rise] = -1.0 + (2.0 * phase[mask_rise] / rise_time)
    
    # Top flat part
    mask_top = (phase >= rise_time) & (phase < rise_time + duty_cycle)
    trapezoid[mask_top] = 1.0
    
    # Falling edge (from 1 to -1)
    fall_start = rise_time + duty_cycle
    mask_fall = (phase >= fall_start) & (phase < fall_start + fall_time)
    trapezoid[mask_fall] = 1.0 - (2.0 * (phase[mask_fall] - fall_start) / fall_time)
    
    # Bottom flat part
    mask_bottom = (phase >= fall_start + fall_time)
    trapezoid[mask_bottom] = -1.0
    
    # Scale by amplitude
    trapezoid *= amplitude
    
    # Convert to 16-bit integer
    trapezoid_int16 = (trapezoid * 32767).astype(np.int16)
    
    return trapezoid_int16, sample_rate

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description='Process and encrypt audio waveforms while preserving musical patterns.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    
    parser.add_argument(
        '-s', '--shape',
        choices=['sine', 'triangle', 'square', 'sawtooth', 'trapezoid'],
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
        '-sr', '--samplerate',
        type=int,
        default=48000,
        help='Sample rate in Hz'
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

def normalize_audio(encrypted_audio, original_audio, wave_shape):
    """
    Normalize encrypted audio to match the amplitude range of the original audio,
    with special handling for different wave shapes.
    
    Args:
        encrypted_audio: The processed/encrypted audio data
        original_audio: The reference audio data to match
        wave_shape: The type of wave being processed ('sine', 'square', 'triangle', etc.)
    """
    # Convert to float for calculations
    encrypted_float = encrypted_audio.astype(np.float32)
    original_float = original_audio.astype(np.float32)
    
    # Different normalization strategies based on wave shape
    if wave_shape == 'square':
        # Square waves: use peak normalization
        original_peak = np.max(np.abs(original_float))
        encrypted_peak = np.max(np.abs(encrypted_float))
        
        if encrypted_peak > 0:
            scaling_factor = original_peak / encrypted_peak
        else:
            scaling_factor = 1.0
            
    elif wave_shape == 'triangle':
        # Triangle waves: use RMS and peak normalization combination
        original_peak = np.max(np.abs(original_float))
        encrypted_peak = np.max(np.abs(encrypted_float))
        original_rms = np.sqrt(np.mean(original_float**2))
        encrypted_rms = np.sqrt(np.mean(encrypted_float**2))
        
        # Use a weighted combination of peak and RMS normalization
        if encrypted_peak > 0 and encrypted_rms > 0:
            peak_scaling = original_peak / encrypted_peak
            rms_scaling = original_rms / encrypted_rms
            # Weight more towards peak normalization for triangle waves
            scaling_factor = 0.7 * peak_scaling + 0.3 * rms_scaling
        else:
            scaling_factor = 1.0
            
    elif wave_shape == 'trapezoid':
        # Trapezoid waves: similar to triangle waves but with different weights
        original_peak = np.max(np.abs(original_float))
        encrypted_peak = np.max(np.abs(encrypted_float))
        original_rms = np.sqrt(np.mean(original_float**2))
        encrypted_rms = np.sqrt(np.mean(encrypted_float**2))
        
        if encrypted_peak > 0 and encrypted_rms > 0:
            peak_scaling = original_peak / encrypted_peak
            rms_scaling = original_rms / encrypted_rms
            # Equal weighting for trapezoid waves
            scaling_factor = 0.5 * peak_scaling + 0.5 * rms_scaling
        else:
            scaling_factor = 1.0
            
    else:
        # For sine and sawtooth waves, use pure RMS normalization
        original_rms = np.sqrt(np.mean(original_float**2))
        encrypted_rms = np.sqrt(np.mean(encrypted_float**2))
        
        if encrypted_rms > 0:
            scaling_factor = original_rms / encrypted_rms
        else:
            scaling_factor = 1.0
    
    # Apply scaling with additional amplitude safety check
    normalized = encrypted_float * scaling_factor
    
    # Additional safety check to prevent any samples from exceeding int16 range
    max_val = np.max(np.abs(normalized))
    if max_val > 32767:
        normalized *= (32767 / max_val)
    
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

    num_chunks = len(audio_data) // CHUNK_SIZE
    trimmed_length = num_chunks * CHUNK_SIZE
    audio_trimmed = audio_data[:trimmed_length]
    chunks = audio_trimmed.reshape(-1, CHUNK_SIZE)
    
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
            sample_rate=args.samplerate
        )
    elif args.shape == 'triangle':
        audio_data, sample_rate = generate_triangle_wave(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
            sample_rate=args.samplerate
        )
    elif args.shape == 'square':
        audio_data, sample_rate = generate_square_wave(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
            sample_rate=args.samplerate
        )
    elif args.shape == 'sawtooth':
        audio_data, sample_rate = generate_sawtooth(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
            sample_rate=args.samplerate
        )
    elif args.shape == 'trapezoid':
            audio_data, sample_rate = generate_trapezoid_wave(
            amplitude=args.amplitude,
            frequency=args.frequency,
            duration=args.duration,
            sample_rate=args.samplerate
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
        mixed_audio = wet_dry_mix(encrypted_audio, original_audio, args.wet)
        
        # Normalize the final audio
        final_audio = normalize_audio(mixed_audio, quantized_audio, args.shape)

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