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
- There is a striking example of this on [Wikipedia](https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation?oldformat=true#Electronic_codebook_(ECB)).
- In the example on the page, the Linux penguin contains uniform areas of color which is easily seen in the processed image.
- Although ECB is not secure at all, I was wondering of its use for music and sounds, and to perhaps create a synth that takes in a sample, encrypts it using ECB and spits it out.
- I did some research on .wav headers, and found a [promising StackOverflow link](https://stackoverflow.com/questions/55420292/remove-file-header-from-a-wav-file-in-python3) about how to extract the header and modify the data in Python. I will do more research later.
- The talk of PCM also had me thinking about my voice recorder app that I released to the [App Store](https://apps.apple.com/us/app/micman/id6615062868).
- After recording a sound, the app uses the file's PCM buffer to extract information about the amplitudes into an array of floats, and that array of floats is used to generated rectangles on the screen with their heights corresponding to the size of the float.
- In detail, what my app does is:
    * Retrieve the PCM buffer of the .m4a file.
    * Extract 128 samples from the buffer. Specifically, it gets the length of the audio file, divides it by 128 and picks values from the buffer using that as a stride value.
    * Calculate the RMS of each sample.
    * Applies a noise floor to the sample.
    * Normalize the samples so that they fit within the range [0, 1].
    * Return an array of floats from that processed data.
    * Uses SwiftUI (Apple's UI framework) to draw a set of 128 rectangles with their heights corresponding to the size of the float.
- I got a lot of help with that code from both online and LLM sources, so before learning about the buffer I wasn't exactly sure what was going on but now I have a clearer picture of what's happening under the hood.

## 10/14/2024
- Introduced myself in the Introductions channel on Zulip.

## 10/15/2024
- Euler's formula kinda reminds me of Fourier's [original sine and cosine trasforms](https://en.wikipedia.org/wiki/Sine_and_cosine_transforms), where the cosine represents the even part of the function (like how in Euler's forumla, the cosine represents the real part), and the sine represents the odd part (in Euler's formula, the sine represents the imaginary part.) I just thought it was an interesting parallel.
- This talk of Fourier transforms reminds me of a project I did in high school.
- In that project, I used Fourier Transforms on the guitar riffs of songs to find out the specific notes that the guitarist was playing.
- Admittedly, I didn't really know what was going on back then but to learn about it again now puts it into context!
- Of course it wasn't very succesful since I forgot about the overtones guitar has as an instrument.
- I tried it again today by playing an E major chord on guitar and the plot spectrum in Audacity was riddled with overtones and other notes not in the chord.
- However, I guess that's what gives guitar its unique and rich sound.
- I tried it once more by playing a single note (E flat/D sharp) and the frequency spectrum was interesting. Although it had a lot of overtones still, a lot of the peaks were D# notes.
- ![Spectrum for D Sharp note](https://i.imgur.com/booFVRy.png)

## 10/29/2024
- Today I got a prototype for the ECB sound encryption working.
- However, it just seems to produce white noise.
- I'll have to investigate further.
- Regarding the tone control assignment, I will look into it this weekend as I'm rather busy this week.
- I think it'll just be an issue of performing an FFT on the sounds, extracting the frequency bins and making filters for the low, mid, and high bands. 

## 11/01/2024
- I'm struggling a bit to figuring out how to work scipy.fft.
- I'm having issues understanding how rfftfreq works.
- I will look more into this tomorrow.

## 11/02/2024
- I realized my FFT approach was going to be quite messy.
- I instead decided to use `scipy.signal.butter` to create the filter.
- I will admit I got a little help from Claude 3.5 with this, I was feeling overwhelmed with the amount of filter options in `scipy.signal`.
- What my code does is create a Butterworth filter for each band, (the bass and treble ones being low and high pass, while the mids are bandpass), returns those filter coefficients, and uses `scipy.signal.filtfilt` to apply those filter coefficients to the .wav file.
- The advantage of using `scipy.signal.filtfilt` is mainly the fact that it does a forward and a backwards pass, which cancels out phase delays, as well as the fact that since it runs twice, it has double the filter order.

## 11/13/2024
- I figured out my ECB effect.
- Turns out in order to get an audible effect, the input sound as to a.) be periodic and b.) be quantized in order to exaggerate patterns. This is making me think that rather than a VST, I want to make a synthesizer using this effect as my project.
- I'm thinking that the parameters be the note, the encryption key (optional) and the wave shape.
- When we learn about synthesis and MIDI I will try that out with my ECB effect (although I'll have to modify it to take a MIDI stream instead of a .wav file.)

## 11/17/2024
- I am researching the MIDI message format so I can figure out how to manipulate the bits to add my ECB effect on it.
- Using [this link](https://www.songstuff.com/recording/article/midi-message-format/), it turns out that there only the note number is transmitted, not anything that directly handles the sound.
- I'll have to rethink my approach. Perhaps I can make a VST effect that takes in the sound? I'm not exactly sure.

## 11/18/2024
- I modified ECBTest.py to become ECBFX.py, the difference being that a.) instead of taking a .wav file as input, it generates a waveform shape based on user input and b.) it now has a wet/dry mix control and c.) the encrypted waveform can now play from the command line.
- It wasn't that hard to implement thanks to `sounddevice` and `argparse`.
- The wet/dry mix is calculated by the formula `out = dry * knobValue + wet * (1 - knobValue)`.
- I think my project is completed for now!
- The usage of it is in its corresponding README.md file.
- After implementing the other wave shapes, I used Claude 3.5 to help me to figure out how to add a trapezoid wave generator, just to add an esoteric wave shape to my code.
- I also used Claude 3.5 to help me with figuring out the normalization for square, trapezoid and triangle waves, as RMS works differently for them.

## 11/23/2024
- I got a sampler working in JUCE, but for some reason the ECB encryption doesn't work. The outputted file is the same as the input.
- I have to figure that out, and then add a wet/dry mix knob and then my project will be complete.

## 11/24/2024
- Somehow, after I resumed working today, the ECB encryption started working by itself. Funny how it works out like that, eh?
- The code uses the OpenSSL library to encrypt the sound.
- I added a wet/dry slider and a text field to change the encryption key.
- The last thing to add is for the notes to loop when a key is held down and pitchwheel support, and then my project is complete.

## 11/25/2024
- I added looping, pitchwheel support, the ability to change the encryption key, quantization to exagerrate patterns, and polyphony.
- Thanks to the help of Claude 3.5 I figured out how to add an envelope to remove clicking on key release.
- Right now the plugin is mono only, and mixes stereo down to mono.
- I will work on a README.md later.

## 11/28/2024
- The plugin works on Mac but doesn't work on Linux.
- I am currently troubleshooting the issue.

## 12/03/2024
- I completed the README for JUCECB.
- I also completed Popgen.
- I added an argument for a chord progression. This wasn't hard to do thanks to argparse and the nifty `nargs` parameter.
- I changed the sine wave to a square wave for that Nintendo Entertainment System sound by using `np.sign` on the sine wave.
- I added drums by the way of manipulating noise samples. This was done by using `np.random` to generate random noise and exponential decay to bring in the crack sound for the snare hit, and using a frequency-modulated sine wave for the kick. There is an exponential decay from 60 Hz to 0 Hz in order to get that characteristic kick "boom". It uses `np.cumsum`to accumulate the frequency over time. It also has an envelope to decrease the volume of the kick quickly.
