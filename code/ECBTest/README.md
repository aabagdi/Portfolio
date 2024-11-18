## ECBTest

- Electronic codebook mode (ECB) is a block cipher mode of encryption. A block cipher mode of encryption is an algorithm that uses a block cipher (a cipher that operates on fixed-length blocks of the input, commonly something like AES) to encrypt something.
- ECB specifically works on the blocks of data by using the same key to independently encrypt each block, and then concatenating the results together. The diagram below depicts how ECB works: ![diagram](https://i.imgur.com/WoEHdRj.png)
- The key thing about any block cipher is that its output is deterministic given the input so that the output can be decrypted to recover the input.
- Thus, ECB is also deterministic due to using the same block cipher for each block.
- However, this determinism leads to some issues regarding the security of ECB: if the given input has a repeating pattern in it, the ciphertext will also have patterns! This is depicted by this ECB encryption of the Linux penguin: ![penguin](https://i.imgur.com/4CzMItx.png)
- This is, obviously, horribly insecure.
- I figured that if ECB works this way on images with patterns in them, it could work with periodic sounds!
- Thus, the cryptographer's chagrin becomes the musiscian's mania: I've harnessed ECB to create an interesting effect on waveforms.
- What this code does is take in a .wav file, and extracts the sample data.
- It then quantizes the samples to exaggerate patterns in them.
- It then splits the samples into 2048-sample chunks.
- It then uses a key to encrypt each chunk.
- It then normalizes the audio to keep the amplitude of the output the same as the input.
- Finally, it writes a new .wav file with the new, encrypted samples.
- This produces an almost buzzsaw-esque distortion on waveforms.
- Here is a sine waveform and its ECB counterpart in Audacity: ![encryption](https://i.imgur.com/icNrWSh.png)

