## 10/8/2024

- Learned how to use NumPy and SciPy to generate a 440 Hz sine wave .wav file.
- Learned how to use the NumPy clip function in order to clamp the audio samples to not exceed a certain threshold.
- I struggled a bit with figuring out how to clip it. However, all I had to do was to add the optional out parameter in clip to data2, which meant that the array would be clipped in place.
- I learned how to output a sound to the speakers using Sounddevice.
- I struggled a bit with this too, as no sound was playing initially, but all I had to do was put `sd.wait()`. This made the code wait until the sound was finished playing.

## 10/10/2024
- I recall learning about something in my cryptography class. It is something called a block cipher, which is a way of encrypting data in blocks.
- How does this relate to Computers, Sound and Music?
- There is a mode of operation called "Electronic Codebook Mode" (ECB). What this does is
    * Break the ciphertext into blocks.
    * Each block is encrypted seperately, using the same function, and the concatenated together.
- However, since data often contains repeating patterns, this often leads to patterns in the ciphertext.
- There is a striking example of this on [Wikipedia](https://www.wikiwand.com/en/articles/Block_cipher_mode_of_operation#Electronic_codebook_(ECB)).
- In the example on the page, the Linux penguin contains uniform areas of color which is easily seen in the processed image.
- Although ECB is not secure at all, I was wondering of its use for music and sounds, and to perhaps create a synth that takes in a sample, encrypts it using ECB and spits it out.
- I did some research on .wav headers, and found a [promising StackOverflow link](https://stackoverflow.com/questions/55420292/remove-file-header-from-a-wav-file-in-python3) about hwo to extract the header and modify the data in Python. I will do more research later.
- The talk of PCM buffers also had me thinking about my voice recorder app that I released to the [App Store](https://apps.apple.com/us/app/micman/id6615062868).
- After recording a sound, the app uses the file's PCM buffer to extract information about the amplitudes into an array of floats, and that array of floats is used to generated rectangles on the screen with their heights corresponding to the size of the float.
- In detail, what my app does is:
    * Retrieve the PCM buffer of the .m4a file.
    * Extract 128 samples from the buffer. Specifically, it gets the length of the audio file, divides it by 128 and uniformly picks samples from the buffer using that value.
    * Calculate the RMS of each sample.
    * Applies a noise floor to the sample.
    * Normalize the samples so that they fit within the range [0, 1].
    * Return an array of floats from that processed data.
    * Uses SwiftUI (Apple's UI framework) to draw a set of 128 rectangles with their heights corresponding to the size of the float.
- I got a lot of help with that code  from both online and LLM sources, so before learning about the buffer I wasn't exactly sure what was going on but now I have a clearer picture of what's happening under the hood.
