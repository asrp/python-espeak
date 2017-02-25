# Python bindings for the eSpeak speech synthesizer
# -*- coding: utf-8 -*-
#
# Python bindings for the eSpeak speech synthesizer
#
# Copyright © 2009-2011 Siegfried-Angel Gevatter Pujals <rainct@ubuntu.com>
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
from . import _repr

# Functions which are already pythonic enough
set_parameter = core.set_parameter
set_voice = core.set_voice
synth = core.synth
set_SynthCallback = core.set_SynthCallback
cancel = core.cancel
get_parameter = core.get_parameter
is_playing = core.is_playing

# Parameter constants
class Parameter:
	Rate = core.parameter_RATE
	Volume = core.parameter_VOLUME
	Pitch = core.parameter_PITCH
	Range = core.parameter_RANGE
	Punctuation = core.parameter_PUNCTUATION
	Capitals = core.parameter_CAPITALS
	Wordgap = core.parameter_WORDGAP

class Punctuation:
	Any = core.punctuation_NONE
	All = core.punctuation_ALL
	Custom = core.punctuation_SOME

# Event constants
event_WORD = core.event_WORD
event_SENTENCE = core.event_SENTENCE
event_MARK = core.event_MARK
event_PLAY = core.event_PLAY
event_END = core.event_END
event_MSG_TERMINATED = core.event_MSG_TERMINATED
event_PHONEME = core.event_PHONEME

class Gender:
	Unknown = 0
	Male = 1
	Female = 2

# From here one we work on making the remaining functions more pythonic

def list_voices():
	"""Returns all available voices."""
	voices = []
	for voice in core.list_voices():
		current_voice = _repr.Voice({
			'name': voice['name'],
			'identifier': voice['identifier'],
			'gender': voice['gender'] if voice['gender'] else None,
			'age': voice['age'] if voice['age'] else None,
			'variant': voice['variant'],
			})
		voices.append(current_voice)
	return voices
