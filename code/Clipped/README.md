## 10/8/2024

- Learned how to use NumPy and SciPy to generate a 440 Hz sine wave .wav file.
- Learned how to use the NumPy clip function in order to clamp the audio samples to not exceed a certain threshold.
- I struggled a bit with figuring out how to clip it. However, all I had to do was to add the optional out parameter in clip to data2, which meant that the array would be clipped in place.
- The code produces a 440 Hz sine tone, a clipped sine tone, and plays back the clipped sound (sounddevice doesn't have a volume control so please turn down your volume for this, I don't want to blast your ears.)
