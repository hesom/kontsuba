import mitsuba as mi
mi.set_variant("cuda_ad_rgb")

scene = mi.load_file("./build/kontsuba/test/scene.xml")

img = mi.render(scene)

mi.util.write_bitmap("test.png", img)