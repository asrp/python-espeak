from setuptools import setup, Extension

module = Extension('espeak.core',
	sources = ['espeak/espeakmodulecore.cpp'],
	libraries = ['espeak'])

setup(
    name = 'python-espeak',
    version = '0.6.3',
    description = 'Python C extension for the eSpeak speech synthesizer',
    author = 'Siegfried-A. Gevatter Pujals, asrp',
    author_email = 'asrp@email.com',
    url = 'https://github.com/asrp/python-espeak',
    license = 'GNU GPL',
    platforms = 'posix',
    ext_modules = [module],
    packages = ['espeak'],
    long_description = open("README.md").read(),
    long_description_content_type="text/markdown",
    )
