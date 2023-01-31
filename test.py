import mitsuba as mi
import matplotlib.pyplot as plt

mi.set_variant("scalar_rgb")

scene = mi.load_file("./build/kontsuba/test/scene.xml")

img = mi.render(scene)
plt.imshow(img ** (1 / 2.2)) # gamma correction
plt.show()