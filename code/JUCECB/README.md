## JUCECB Background
- Electronic codebook mode (ECB) is a block cipher mode of encryption. A block cipher mode of encryption is an algorithm that uses a block cipher (a cipher that operates on fixed-length blocks of the input, commonly something like AES) to encrypt something.
- ECB specifically works on the blocks of data by using the same key to independently encrypt each block, and then concatenating the results together. The diagram below depicts how ECB works: ![diagram](https://i.imgur.com/WoEHdRj.png)
- The key thing about any block cipher is that its output is deterministic given the input so that the output can be decrypted to recover the input.
- Thus, ECB is also deterministic due to using the same block cipher for each block.
- However, this determinism leads to some issues regarding the security of ECB: if the given input has a repeating pattern in it, the ciphertext will also have patterns! This is depicted by this ECB encryption of the Linux penguin: ![penguin](https://i.imgur.com/4CzMItx.png)
- This is, obviously, horribly insecure, and is why ECB isn't used in modern cryptography.
- I figured that if ECB works this way on images with patterns in them, it could work with periodic sounds!
- Thus, the cryptographer's chagrin becomes the musiscian's merriment: I've harnessed ECB to create an interesting effect on waveforms.
- What this code does is generate an audio buffer given a .wav file.
- It then mixes it down to mono if it's a stereo file.
- It then normalizes and quantizes the input.
- It then uses ECB to create an encrypted audio buffer.
- It then normalizes the encrypted buffer.
- It then does a wet/dry mix.
- Finally, it plays the resulting sound with the correct pitch when the corresponding key is pressed, with an envelope to prevent clicking.
- This produces an almost buzzsaw-esque distortion on waveforms.
- This code also handles polyphony, by using a custom Voice struct that contains all the corresponding parameters for MIDI playback (MIDI note number, playback rate, etc) and also holds data needed to calculate the envelope (attack time, release time, etc.) It uses an array to store a max of 4 of these voice structs at a time, and has note stealing.
## Interface
![interface](https://i.imgur.com/qYo9YiP.png)
- Load .wav file: Loads a .wav file
- Dry/Wet: Controls the dry/wet mix. 0 is totally dry, 1 is totally wet.
- Gain: Gain control
- Encryption key: The key used for encrypting samples. Play aronud with this to get slightly different sounds!
- Loop: If enabled, loop the loaded .wav file when the key is held down.
- There are sound samples int the 'Sound Sample' folder if you want to see what the plugin sounds like.
