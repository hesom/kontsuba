# Kontsuba
Kontsuba is a command line tool for converting common 3D scene formats to a Mitsuba 3 scene description.
**Under heavy construction. Chances are high that this doesn't work for you yet**

## Installation
Clone the repository and all dependencies with
```bash
git clone git@github.com:hesom/kontsuba.git --recursive
```
and build the command line tool:
```bash
cd kontsuba
cmake -S . -B build
cmake --build build
```

You can alternatively build and install a Python extension by just invoking `pip install .` in the project's root directory. Currently this is pretty much useless and just a way to use the converter inside a Python script without a subprocess call. See `test.py` for a usage example.

## Usage
```bash
./kontsuba <input-file> <output-directory>
```
converts a scene at `<input-file>` into a Mitsuba 3 compatible scene description in `<output-directory>`. The xml file required by Mitsuba is located at `<output-directory>/scene.xml`. Meshes are split by material and placed `meshes` subfolder in `.ply` format.
Kontsuba in principle works with every file format that can be loaded by [Assimp](https://github.com/assimp/assimp/blob/master/doc/Fileformats.md)

## Limitations / TODO
- All materials are converted to the `principled` BSDF plugin. While this is the most flexible BSDF in Mitsuba, it might be not the most efficient one.
- Non-PBR materials are simply converted by using the default BSDF parameters if no corresponding parameters where found in the input file. For example, all parameters of Phong materials are ignored, except for the diffuse color, which is used as the `base_color` parameter of the principled BSDF.
- There are currently no command line options for converting between left/right-handed coordinate systems or flipping uv-coordinates, which might be necessary depending on the input.
- All BSDFs are `twosided`.
- Bump/normal mapping, spectral and polarized materials and blended BSDFs are not supported yet.
- Custom shaded materials simply don't work. This includes [texture stacks](https://assimp.sourceforge.net/lib_html/materials.html) that are more complex than a single layer.
- Meshes are exported as `.ply` files but the `serialized` plugin format by Mitsuba would be more efficient.
