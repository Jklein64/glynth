import numpy as np
import matplotlib.pyplot as plt

tensor = np.load("out/tensor.npy")
x, y = tensor
copper = plt.get_cmap("copper")
colors = copper(np.linspace(0, 1, tensor.shape[1]))
fig, axs = plt.subplots(ncols=2)
axs[0].scatter(x, y, marker=".", c=colors)
axs[1].plot(x)
axs[1].plot(y)
plt.show()
