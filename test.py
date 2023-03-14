import mitsuba as mi
import matplotlib.pyplot as plt
import kontsuba

mi.set_variant("scalar_rgb")

kontsuba.convert("./test_models/shapenet/models/model_normalized.obj", "tmp") # TODO directly return Python dict
scene = mi.load_file("./tmp/scene.xml")



#params = mi.traverse(scene)
#print(params)


img = mi.render(scene)
plt.imshow(img ** (1 / 2.2)) # gamma correction
plt.show()