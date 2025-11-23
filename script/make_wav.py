import numpy as np
from scipy.io import wavfile

outline = np.load("out/outline.npy")
# normalize
outline -= np.mean(outline)
outline /= np.max(np.abs(outline))
outline *= 0.1
waveform = np.tile(outline, (200, 1))
wavfile.write("out/waveform.wav", rate=44100, data=waveform.astype(np.float32))
