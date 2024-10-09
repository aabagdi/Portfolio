from scipy.io.wavfile import write
import numpy as np
import sounddevice as sd

sampleRate = 48000
freq = 440

amp1 = 8192
amp2 = 16384

range = np.linspace(0., 1., sampleRate)

data1 = amp1 * np.sin(2. * np.pi * freq * range)
data2 = amp2 * np.sin(2. * np.pi * freq * range)

np.clip(data2, -8192, 8192, out = data2)

sd.play(data2, sampleRate)
sd.wait()
write("sine.wav", sampleRate, data1.astype(np.int16))
write("clipped.wav", sampleRate, data2.astype(np.int16))