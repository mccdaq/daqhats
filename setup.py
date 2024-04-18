#!/usr/bin/env python

import sys
from setuptools import setup

__project__ = 'daqhats'
__version__ = '1.5.0.0'
__description__ = 'MCC DAQ HAT Python Library'
__packages__ = ['daqhats']
__author__ = 'Measurement Computing Corp.'
__author_email__ = 'info@mccdaq.com'
__url__ = 'https://github.com/mccdaq/daqhats'
__license__ = 'MIT'

if sys.version_info < (3,4):
    __install_requires__ = ['enum34']
else:
    __install_requires__ = []

setup(
    name = __project__,
    version = __version__,
    description = __description__,
    author = __author__,
    author_email = __author_email__,
    license = __license__,
    url = __url__,
    packages = __packages__,
    install_requires = __install_requires__
)
