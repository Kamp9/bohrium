#!/usr/bin/env python
"""
/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
http://bohrium.bitbucket.org

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/
"""

from distutils.core import setup, Extension
from distutils.command.build import build
import os
import sys
import stat
import pprint
import json
import numpy as np
from Cython.Build import cythonize

#We overload the setup.py with a 'buildpath=' argument that
#points to the root of the current build
build_path = None
for i,arg in enumerate(sys.argv):
    if arg.startswith("buildpath="):
        build_path = arg[len("buildpath="):]
        sys.argv.pop(i)

def buildpath(*args):
    if build_path is None:
        return os.path.join(*args)
    else:
        return os.path.join(build_path, *args)

def srcpath(*args):
    prefix = os.path.abspath(os.path.dirname(__file__))
    assert len(prefix) > 0
    return os.path.join(prefix, *args)

def get_timestamp(f):
    st = os.stat(f)
    mtime = st[stat.ST_MTIME] #modification time
    return mtime

def set_timestamp(f,timestamp):
    os.utime(f,(timestamp,timestamp))

#Returns the numpy data type name
def dtype_bh2np(bh_type_str):
    return bh_type_str[3:].lower()#Remove BH_ and convert to lower case

#Merge bhc.i.head with the bh_c.h to create our SWIG interface bhc.i
time = 0
with open(buildpath("bhc.i"), 'w') as outfile:
    for fname in [srcpath("bhc.i.head"),srcpath("..","c","codegen","output","bh_c.h")]:
        t = get_timestamp(fname)
        if t > time:
            time = t
        with open(fname) as infile:
            for line in infile:
                outfile.write(line)
set_timestamp(buildpath("bhc.i"),time)

#Create the _info.py file
time = get_timestamp(srcpath('setup.py'))
with open(buildpath("_info.py"), 'w') as o:
    #Write header
    o.write("#This file is auto generated by the setup.py\n")
    o.write("import numpy as np\n")

    #Find number of operands and type signature for each Bohrium opcode
    #that Bohrium-C supports
    t = get_timestamp(srcpath('..','..','core','codegen','opcodes.json'))
    if t > time:
        time = t
    nops = {}
    type_sig = {}

    ufunc = {}
    with open(srcpath('..','..','core','codegen','opcodes.json'), 'r') as f:
        opcodes = json.loads(f.read())
        for op in opcodes:
            if op['elementwise'] and not op['system_opcode']:
                #Convert the type signature to bhc names
                type_sig = []
                for sig in op['types']:
                    type_sig.append([dtype_bh2np(s) for s in sig])

                name = op['opcode'].lower()[3:]#Removing BH_ and we have the NumPy and bohrium name
                ufunc[name] = {'name':     name,
                               'nop':      int(op['nop']),
                               'type_sig': type_sig}
    o.write("op = ")
    pp = pprint.PrettyPrinter(indent=2, stream=o)
    pp.pprint(ufunc)

    #Find and write all supported data types
    t = get_timestamp(srcpath('..','..','core','codegen','types.json'))
    if t > time:
        time = t
    s = "numpy_types = ["
    with open(srcpath('..','..','core','codegen','types.json'), 'r') as f:
        types = json.loads(f.read())
        for t in types:
            if t['numpy'] == "unknown":
                continue
            s += "np.dtype('%s'), "%t['numpy']
        s = s[:-2] + "]\n"
    o.write(s)
set_timestamp(buildpath("_info.py"),time)

#We need to make sure that the extensions is build before the python module because of SWIG
#Furthermore, '_info.py' and 'bhc.py' should be copied to the build dir
class CustomBuild(build):
    sub_commands = [
        ('build_ext', build.has_ext_modules),
        ('build_py', build.has_pure_modules),
        ('build_clib', build.has_c_libraries),
        ('build_scripts', build.has_scripts),
    ]
    def run(self):
        if not self.dry_run:
            self.copy_file(buildpath('_info.py'),buildpath(self.build_lib,'bohrium','_info.py'))
            self.copy_file(buildpath('bhc.py'),buildpath(self.build_lib,'bohrium','bhc.py'))
        build.run(self)

setup(name='Bohrium',
      version='0.2',
      description='Bohrium NumPy',
      long_description='Bohrium NumPy',
      author='The Bohrium Team',
      author_email='contact@bh107.org',
      url='http://www.bh107.org',
      license='LGPLv3',
      platforms='Linux, OSX',
      cmdclass={'build': CustomBuild},
      package_dir={'bohrium': srcpath('')},
      packages=['bohrium', 'bohrium.examples'],
      ext_package='bohrium',
      ext_modules=[Extension(name='_bhmodule',
                             sources=[srcpath('src','_bhmodule.c')],
                             depends=[srcpath('src','types.c'), srcpath('src','types.h'),
                                      srcpath('src','operator_overload.c')],
                             include_dirs=[srcpath('..','c','codegen','output'),
                                           srcpath('..','..','include')],
                             libraries=['dl','bhc', 'bh'],
                             library_dirs=[buildpath('..','c'),
                                           buildpath('..','..','core')],
                             ),
                   Extension(name='_bhc',
                             sources=[buildpath('bhc.i')],
                             include_dirs=[srcpath('..','c','codegen','output'),
                                           srcpath('..','..','include')],
                             libraries=['dl','bhc', 'bh'],
                             library_dirs=[buildpath('..','c'),
                                           buildpath('..','..','core')],
                             )] +
                   cythonize([Extension(name='_random123',
                             sources=[srcpath('r123','_random123.pyx')],
                             include_dirs=[srcpath('.'),
                                           srcpath('..','..','thirdparty','Random123-1.08','include')],
                             libraries=[],
                             library_dirs=[],
                             )])
     )
