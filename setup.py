#!/usr/bin/env python

from setuptools import setup

setup(
    name='daqhats',
    version='1.0.0',
    description='MCC DAQ HAT Python Library',
    author='Measurement Computing Corp.',
    #author_email='',
    license='MIT',
    url='https://github.com/mccdaq/daqhats',
    packages=['daqhats'],
    install_requires=['enum34;python_version<"3.4"']
)
