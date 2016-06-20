#!/usr/bin/env python

# Two environmental variables influence this script.
#
# GDAL_CONFIG: the path to a gdal-config program that points to GDAL headers,
# libraries, and data files.
#
# PACKAGE_DATA: if defined, GDAL and PROJ4 data files will be copied into the
# source or binary distribution. This is essential when creating self-contained
# binary wheels.

import itertools
import logging
import os
import pprint
import shutil
import subprocess
import sys

from setuptools import setup
from setuptools.extension import Extension


logging.basicConfig()
log = logging.getLogger()



# Parse the version from the greyhound module.
with open('greyhound/__init__.py') as f:
    version = ''
    for line in f:
        if line.find("__version__") >= 0:
            version = line.split("=")[1].strip()
            version = version.strip('"')
            version = version.strip("'")
            continue

    with open('VERSION.txt', 'w') as f:
        f.write(version)

with open('README.rst') as f:
    readme = f.read()

# Runtime requirements.
inst_reqs = ['mapbox', 'laspy', 'numpy', ]

setup_args = dict(
    name='greyhound',
    version=version,
    description="Convenience API for interacting with Greyhound point cloud web services",
    long_description=readme,
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Intended Audience :: Information Technology',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Topic :: Multimedia :: Graphics :: Graphics Conversion',
        'Topic :: Scientific/Engineering :: GIS'],
    keywords='point clouds, LiDAR',
    author='Howard Butler',
    author_email='howard@hobu.co',
    url='http://greyhound.io',
    license='BSD',
    package_dir={'': '.'},
    packages=['greyhound'],
    entry_points={'console_scripts':['greyhound-cli=greyhound.main:entry'],},
    include_package_data=True,
    zip_safe=False,
    install_requires=inst_reqs)

setup(**setup_args)

