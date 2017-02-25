This is a modified version of python-espeak ([previous version here](https://launchpad.net/python-espeak)). It is a Python binding over the [eSpeak speech synthesizer](http://espeak.sourceforge.net/) C library and does not simply make calls to the `espeak` binary.

## Changes

Some highlights of the modifications.

- File output instead of only playback (`espeak.init(playing=False)`).
- Support for multiple instances of speakers (the `espeak.Espeak` class). They have to take turns and *not* simultaneously speak.
- Easily setup multiple callbacks (`espeak.add_callback`).
- Wave file content sent through callbacks (for `playing=False` only since this is how the C library works).
- Slightly more uniform bindings in some places (`espeak.const`).
- Optional start and end position for synthesis.

Unfortunately some of the renamings make this version not backwards compatible. (Maybe this library should be named differently. I will try to contact the original author about that. Their email "rainct ubuntu com" seems defunct.) The big one is that `espeak.init()` now has to be called before anything else.

## Compatibility

This library should (still) work with both Python 2 and 3.

## Installation

Requires `espeak` and its libraries to be installed `espeak/speak_lib.h` should be in your include path somewhere. Install with

    python setup.py install

or `python setup.py build` to get the library in the `build` without installation.

## Examples

Simple usage

    import espeak
    espeak.init()
    speaker = espeak.Espeak()
    speaker.say("Hello world")
    speaker.rate = 300
    speaker.say("Faster hello world")

See `espeak.const['parameter']` for all the values other than `rate` that can be changes.

Callback usage

    def print_callback(*args):
        print args

    import espeak
    espeak.init()
    speaker = espeak.Espeak()
    speaker.add_callback(print_callback)
    speaker.say("Hello world")

This should print something like

    ('', 2, 1, 0, 0, None)
    ('', 0, 1, 0, 0, None)
    ('', 1, 1, 5, 0, None)
    ('', 0, 1, 5, 0, None)
    ('', 1, 7, 5, 0, None)
    ('', 0, 7, 5, 0, None)
    ('', 5, 11, 0, 0, None)
    ('', 0, 11, 0, 0, None)

## Callback format

The arguments of callback functions are `wave_file_fragment, event, current_pos, length, num_samples, name`.

- `event` can be reverse looked up in (or compare to) `espeak.const['event']`.
- `wave_file_fragment` is a piece of a wave file (only in `playback=False` mode, otherwise its always the empty string).
- `current_pos` is `event->text_position` from `speak_lib` the C library
- `length` is `event->length` from `speak_lib`
- `num_samples` is `event->length` from `speak_lib`
- `name` is either
    - `(event->id).name` for `play` and `mark` events,
    - `event->id.number` (for `sample_rate` events) or
    - `None` otherwise.

## Licence

GPL v3. See `COPYING` for full text.
