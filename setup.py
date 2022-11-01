#!/usr/bin/env python

import sys
from setuptools import setup

if sys.version_info < (3,4):
    install_requires=['enum34']
else:
    install_requires=[]

setup(
    name='daqhats',
    version='1.4.0.6',
    description='MCC DAQ HAT Python Library',
    author='Measurement Computing Corp.',
    author_email='info@mccdaq.com',
    license='MIT',
    url='https://github.com/mccdaq/daqhats',
    packages=['daqhats'],
    install_requires=install_requires
)
