# -*- coding: utf-8 -*-
#
# Python bindings for the eSpeak speech synthesizer
#
# Copyright © 2009-2011 Siegfried-Angel Gevatter Pujals <rainct@ubuntu.com>
# Copyright © 2015-2017 asrp <asrp@email.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from . import core
from .core import set_voice, synth, init, list_voices
from .core import stop as pause, playing
import time
import atexit
import struct
import sys

def get_wave_filename():
    return _wave_filename

def set_wave_filename(new_filename):
    global _wave_filename
    _wave_filename = new_filename
    core.set_wave_filename(_wave_filename)

set_wave_filename("/tmp/espeak-wave")

const = {"gender": {"unknown": 0, "male": 1, "female": 2}}
for name in dir(core):
    if not name.startswith("__") and not callable(getattr(core, name)):
        paramtype, paramvalue = name.split("_", 1)
        if paramtype not in const:
            const[paramtype] = {}
        const[paramtype][paramvalue.lower()] = getattr(core, name)

DEFAULT = 0
CURRENT = 1
default = {param:core.get_parameter(param_num, DEFAULT)
           for param, param_num in const["parameter"].items()}
default['punctuation'] = "none"
funcmap = {"pause": pause, "synth": synth}
event_name = {v:k for k, v in const["event"].items()}

def set_parameter(key, value):
    if key in ['punctuation']:
        value = const[key][value]
    return core.set_parameter(const["parameter"][key], value)

# Current speaker. There should only be one at a time.
# New speakers should either be queued or interrupt the current speaker.
# None indicates a direct call and not a speaker from a Class.
# Terrible design, but needed if we don't want to change how speak_lib works
current_speaker = None
callbacks = {None: []}
def all_callbacks(event_type, position, length, num_samples, name):
    try:
        if num_samples:
            wave_string = open(_wave_filename, "rb").read()
        else:
            wave_string = b""
        for callback in callbacks.get(current_speaker, [])[:]:
            callback(wave_string, event_type, position, length, num_samples, name)
    except Exception as e:
        import traceback
        print(traceback.print_exc())
    return True

def add_callback(callback, speaker = None):
    if speaker not in callbacks:
        callbacks[speaker] = []
    callbacks[speaker].append(callback)

def remove_callback(callback, speaker = None):
    callbacks[speaker].remove(callback)

core.set_synth_callback(all_callbacks)

def set_speaker(speaker):
    global current_speaker
    if playing():
        pause()
    current_speaker = speaker

WAVE_FORMAT_PCM = 0x0001

# Taken from wave.py
# Set to espeak's defaults
def wave_header(init_length = 0x7ffff000, num_frames = None, num_channels = 1,
                sample_width = 2, framerate = 22050):
    """
    num_frames -- the number of audio frames written to the header
                  set through the setnum_frames() or setparams() method
    """
    # Not sure why 0x7ffff000 is the default for speak and espeak
    # Maybe an arbitrary large number would do
    datalength = num_frames * num_channels * sample_width if num_frames else init_length
    header = struct.pack(b'<4sL4s4sLHHLLHH4sL',
                         b'RIFF', 36 + datalength, b'WAVE', b'fmt ', 16,
                         WAVE_FORMAT_PCM, num_channels, framerate,
                         num_channels * framerate * sample_width,
                         num_channels * sample_width,
                         sample_width * 8,
                         b'data', datalength)
    return header

class Espeak(object):
    def __init__(self):
        self.__dict__["param"] = default.copy()
        self.voice = {"language": "en"}

    def add_callback(self, callback):
        add_callback(callback, self)

    def say(self, *args, **kwargs):
        # TODO: Find a better test
        if sys.version_info[0] == 2:
            args = [arg.encode('utf-8') if type(arg) == unicode else arg
                    for arg in args]
        set_speaker(self)
        for key, value in self.param.items():
            set_parameter(key, value)
        set_voice(**self.voice)
        self.synth(*args, **kwargs)

    def playing(self):
        return (self == current_speaker and playing())

    def set_voice(self, **kwargs):
        """ Use this function to trigger immediate change.
        Otherwise, use self.voice[param] = value"""
        self.voice.update(kwargs)
        if self == current_speaker:
            set_voice(**self.voice)

    def __getattr__(self, key):
        if key in funcmap:
            return funcmap[key]
        elif key in self.__dict__["param"]:
            return self.param[key]
        else:
            return object.__getattribute__(self, key)

    def __setattr__(self, key, value):
        if key in self.param:
            self.param[key] = value
            if self == current_speaker:
                return set_parameter(key, value)
        else:
            object.__setattr__(self, key, value)

def unload_module():
    # Need to troubleshoot why this is needed to avoid a segfault
    # Segfault only happens in py3, not py2!
    global callbacks
    del callbacks

atexit.register(unload_module)

# Uncomment to automatically initialize on import
# core.init()

if __name__ == "__main__":
    esp = Espeak()
    esp.rate = 300
    print("Rate set to %s" % esp.rate)
    esp.say("Hello world")
