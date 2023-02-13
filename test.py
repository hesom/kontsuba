import mitsuba as mi
import matplotlib.pyplot as plt
import kontsuba

mi.set_variant("scalar_rgb")

kontsuba.convert("./test_models/normalTest.gltf", "tmp") # TODO directly return Python dict
scene = mi.load_file("./tmp/scene.xml")

img = mi.render(scene)
plt.imshow(img ** (1 / 2.2)) # gamma correction
plt.show()