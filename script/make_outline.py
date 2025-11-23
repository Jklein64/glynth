import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
from numpy.lib.stride_tricks import sliding_window_view

cmap = mpl.colormaps.get_cmap("copper")

outline = np.load("out/outline.npy")
lengths = np.zeros(outline.shape[0] - 1)
fig, axs = plt.subplots(ncols=2)
fig.set_size_inches(12, 4)
for i, line in enumerate(sliding_window_view(outline, (2, 2))):
    p1, p2 = np.squeeze(line)
    lengths[i] = np.linalg.norm(p2 - p1)
colors = cmap(lengths / np.max(lengths))
for i, line in enumerate(sliding_window_view(outline, (2, 2))):
    line = np.squeeze(line)
    axs[0].plot(line[:, 0], line[:, 1], color=colors[i], zorder=1)

x, y = outline.T
axs[0].scatter(x, y, marker=".", c="black", s=2, zorder=2)
axs[1].plot(np.cumsum(lengths) / np.sum(lengths))
axs[1].set_ylabel("cumulative length proportion")
plt.show()
