import numpy as np
from scipy.io import wavfile
from Crypto.Cipher import AES
import sys
import wave

def process_audio_file(input_file):
    """Load and prepare audio file"""
    try:
        # First check WAV file properties
        with wave.open(input_file, 'rb') as wav:
            channels = wav.getnchannels()
            sample_width = wav.getsampwidth()
            sample_rate = wav.getframerate()
            print(f"Input WAV properties:")
            print(f"- Sample rate: {sample_rate} Hz")
            print(f"- Channels: {channels}")
            print(f"- Sample width: {sample_width * 8} bits")
        
        # Read the WAV file
        sample_rate, audio_data = wavfile.read(input_file)
        
        # Ensure we have int16 data
        if audio_data.dtype != np.int16:
            print("Converting to 16-bit audio...")
            audio_data = (audio_data * 32767).astype(np.int16)
        
        return audio_data, sample_rate
    
    except Exception as e:
        print(f"Error processing audio file: {e}")
        sys.exit(1)

def quantize_audio(audio_data, levels=32):
    """Quantize audio into distinct levels to create more visible patterns"""
    # Convert to float between -1 and 1
    float_data = audio_data.astype(np.float32) / 32768.0
    
    # Quantize to specified number of levels
    quantized = np.round(float_data * (levels/2)) * (2.0/levels)
    
    # Convert back to int16
    return (quantized * 32768).astype(np.int16)

def reshape_for_encryption(audio_data):
    """Reshape audio into larger chunks for pattern preservation"""
    # Use larger chunks - 2048 samples per chunk (4096 bytes)
    chunk_size = 2048
    
    # Handle both mono and stereo
    if len(audio_data.shape) == 1:
        # Mono audio
        num_chunks = len(audio_data) // chunk_size
        trimmed_length = num_chunks * chunk_size
        audio_trimmed = audio_data[:trimmed_length]
        chunks = audio_trimmed.reshape(-1, chunk_size)
    else:
        # Stereo audio
        num_chunks = audio_data.shape[0] // chunk_size
        trimmed_length = num_chunks * chunk_size
        audio_trimmed = audio_data[:trimmed_length, :]
        # Reshape each channel separately
        left_chunks = audio_trimmed[:, 0].reshape(-1, chunk_size)
        right_chunks = audio_trimmed[:, 1].reshape(-1, chunk_size)
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
    cipher = AES.new(key, AES.MODE_ECB)
    
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

def main():
    if len(sys.argv) != 2:
        print("Usage: python script.py <input_wav_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_prefix = input_file.rsplit('.', 1)[0]
    
    # Process input file
    audio_data, sample_rate = process_audio_file(input_file)
    
    # Check if stereo
    is_stereo = len(audio_data.shape) > 1
    
    # Quantize the audio
    quantized_audio = quantize_audio(audio_data)
    
    # Reshape into chunks
    audio_chunks, trimmed_length = reshape_for_encryption(quantized_audio)
    
    # Encrypt chunks
    key = b'4321432143214321'
    encrypted_chunks = process_chunks(audio_chunks, key)
    
    # Prepare final audio data
    if is_stereo:
        # Combine stereo channels
        encrypted_audio = np.column_stack((
            encrypted_chunks[0].flatten(),
            encrypted_chunks[1].flatten()
        ))
        # Trim to original length if needed
        if encrypted_audio.shape[0] > trimmed_length:
            encrypted_audio = encrypted_audio[:trimmed_length, :]
    else:
        # Flatten mono audio
        encrypted_audio = encrypted_chunks.flatten()
        # Trim to original length if needed
        if len(encrypted_audio) > trimmed_length:
            encrypted_audio = encrypted_audio[:trimmed_length]
    
    try:
        # Save encrypted version
        wavfile.write(f"{output_prefix}_encrypted.wav", sample_rate, encrypted_audio)
        print(f"Successfully created: {output_prefix}_encrypted.wav")
        print(f"Output maintains original sample rate of {sample_rate} Hz")
        print(f"Channels: {'Stereo' if is_stereo else 'Mono'}")
    except Exception as e:
        print(f"Error saving encrypted audio: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()