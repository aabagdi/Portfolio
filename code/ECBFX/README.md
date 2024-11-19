## ECBFX Background
- Electronic codebook mode (ECB) is a block cipher mode of encryption. A block cipher mode of encryption is an algorithm that uses a block cipher (a cipher that operates on fixed-length blocks of the input, commonly something like AES) to encrypt something.
- ECB specifically works on the blocks of data by using the same key to independently encrypt each block, and then concatenating the results together. The diagram below depicts how ECB works: ![diagram](https://i.imgur.com/WoEHdRj.png)
- The key thing about any block cipher is that its output is deterministic given the input so that the output can be decrypted to recover the input.
- Thus, ECB is also deterministic due to using the same block cipher for each block.
- However, this determinism leads to some issues regarding the security of ECB: if the given input has a repeating pattern in it, the ciphertext will also have patterns! This is depicted by this ECB encryption of the Linux penguin: ![penguin](https://i.imgur.com/4CzMItx.png)
- This is, obviously, horribly insecure, and is why ECB isn't used in modern cryptography.
- I figured that if ECB works this way on images with patterns in them, it could work with periodic sounds!
- Thus, the cryptographer's chagrin becomes the musiscian's merriment: I've harnessed ECB to create an interesting effect on waveforms.
- What this code does is first generate waveform samples (either a sine, triangle, square, or sawtooth wave.)
- It then quantizes the samples to exaggerate patterns in them.
- It then splits the samples into 2048-sample chunks.
- It then uses a key to encrypt each chunk.
- It then does a wet/dry mix.
- It then normalizes the encrypted audio to keep the amplitude the same as the original generated waveform.
- Finally, it either writes a new .wav file with the new, encrypted samples or plays from the command line.
- This produces an almost buzzsaw-esque distortion on waveforms.
- Here is a sine waveform and its ECB counterpart in Audacity: ![encryption](https://i.imgur.com/JWr14LT.png)

## Usage
- First run `pip install -r requirements.txt`.
- After installing the dependencies, use ECBFX as such: `ECBFX.py [-h] -s {sine,triangle,square,sawtooth} [-a AMPLITUDE] [-f FREQUENCY] [-d DURATION] [-sr SAMPLERATE] [-k KEY] [-w WET] [-o OUTPUT]`
- `-h`: Show help message.
- `-s`: Wave shape, either sine, triangle, square, or sawtooth.
- `-a`: Amplitude, from 0 to 1, default 0.5.
- `-f`: Frequency, default 440 Hz.
- `-d`: Duration in seconds, default 3.
- `-sr`: Sample rate in Hz, default 48000. Decreasing this value makes the effect more abrasive, while increasing it makes it less abrasive.
- `-k`: Encryption key, has to be 16 characters long. Default is 4321432143214321. Experiment with this to get slightly different sounds!
- `-w`: Wet/dry knob, from 0 to 100 where 100 is fully wet, default 50.
- `-o`: Optionally output the waveform to a .wav file with the specified filename. If not provided, the waveform will play from the terminal.

