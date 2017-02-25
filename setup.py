#! /usr/bin/env python

from distutils.core import setup, Extension

module = Extension('espeak.core',
	sources = ['espeak/espeakmodulecore.cpp'],
	libraries = ['espeak'])

setup(
    name = 'python-espeak',
    version = '0.5',
    description = 'Python bindings for the eSpeak speech synthesizer',
    author = 'Siegfried-A. Gevatter Pujals',
    author_email = 'rainct@ubuntu.com',
    url = 'https://launchpad.net/python-espeak',
    license = 'GPL',
    platforms = 'posix',
    ext_modules = [module],
    packages = ['espeak'],
    )
