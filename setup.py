
import sys, re, os

try:
    from skbuild import setup
except ImportError:
    print("The preferred way to invoke 'setup.py' is via pip, as in 'pip "
          "install .'. If you wish to run the setup script directly, you must "
          "first install the build dependencies listed in pyproject.toml!",
          file=sys.stderr)
    raise

setup(
    name="kontsuba",
    version="0.0.1",
    author="Hendrik Sommerhoff",
    author_email="hendrik.sommerhoff@uni-siegen.de",
    description="Kontsuba - A 3D model converter",
    url="https://github.com/hesom/kontsuba",
    license="MIT",
    packages=['kontsuba'],
    package_dir={'': 'kontsuba/bindings'},
    cmake_install_dir="kontsuba/bindings/kontsuba",
    cmake_languages=("C", "CXX"),
    include_package_data=True,
    python_requires=">=3.9"
)