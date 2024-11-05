from Crypto.Cipher import AES
import sys
import os

filename = sys.argv[1]

binarySound = bytearray()
binaryHeader = bytearray()

song = bytearray()

with open(filename, "rb") as f:
    binaryHeader = f.read(44)
    binarySound = f.read()

keyLength = len(binarySound)

key = os.urandom(32)

print(len(key))
print(len(binarySound))

cipher = AES.new(key, AES.MODE_ECB)

encryptedFile = cipher.encrypt(binarySound)

with open("header.bin", "wb") as f:
    f.write(binaryHeader)

with open("data.bin", "wb") as f:
    f.write(encryptedFile)

with open("header.bin", "rb") as h:
    song = h.read()
    with open("data.bin", "rb") as d:
        song += d.read()

with open("processed.wav", "wb") as f:
    f.write(song)

os.remove("header.bin")
os.remove("data.bin")