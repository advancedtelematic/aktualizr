from distutils.core import setup, Extension
from Cython.Build import cythonize

ext = Extension("rvimodule", 
        sources=["rvimodule.pyx"],
        libraries=["rvi"],
        extra_compile_args=["-fopenmp", "-O3", "-I../include"],
        extra_link_args=["-L../src/.libs/"]
        )

setup(
        name="rvimodule",
        ext_modules=cythonize([ext])
)
