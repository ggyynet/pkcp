from distutils.core import setup, Extension

MOD = 'pkcp'
# define the extension module
cos_module = Extension('pkcp._pkcp', sources=['pkcp.c', 'kcp/ikcp.c'])

# run the setup
setup(
    name=MOD,
    version='0.2',
    description="Python kcp software package",
    author='runbao2013',
    author_email='runbao2013@gmail.com',
    url='https://github.com/ggyynet/pkcp',
    license='MIT',
    packages=['pkcp'],
    package_dir={'': 'src'},
    ext_modules=[cos_module],
)
