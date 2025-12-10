import numpy as np
from scipy.io import wavfile

outline = np.load("out/tensor.npy")[0]

waveform = np.zeros(44100 * 2)
angle = 0
inc = 440 / 44100
n = outline.shape[0]
for i in range(len(waveform)):
    waveform[i] = outline[int(angle * n) % n]
    angle += inc
# normalize
# outline -= np.mean(outline)
# outline /= np.max(np.abs(outline))
# outline *= 0.1
# waveform = np.tile(outline, (200, 1))
wavfile.write("out/waveform.wav", rate=44100, data=waveform.astype(np.float32))
