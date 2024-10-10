from scipy.io.wavfile import write
import numpy as np
import sounddevice as sd

sampleRate = 48000
freq = 440

amp1 = 8192
amp2 = 16384

range = np.linspace(0., 1., sampleRate)

sine = amp1 * np.sin(2. * np.pi * freq * range)
clipped = amp2 * np.sin(2. * np.pi * freq * range)

np.clip(clipped, -8192, 8191, out = clipped)

sd.play(clipped, sampleRate)
sd.wait()

write("sine.wav", sampleRate, sine.astype(np.int16))
write("clipped.wav", sampleRate, clipped.astype(np.int16))