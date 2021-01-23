/*
 * Python bindings for the eSpeak speech synthesizer
 *
 * Copyright © 2009-2012 Siegfried-A. Gevatter Pujals <rainct@ubuntu.com>
 * Copyright © 2009-2011 Joe Burmeister <joe.a.burmeister@googlemail.com>
 * Copyright © 2015-2017 asrp <asrp@email.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Python.h>
#include <espeak/speak_lib.h>
// #include <espeak-ng/speak_lib.h>

#if PY_MAJOR_VERSION < 3
	#define PyLong_FromLong PyInt_FromLong
#endif

// Exception prototypes
static PyObject *BufferFullError;
static PyObject *CallBack = NULL;
static volatile int Stopping = 0;
static volatile int AsyncPython = 0;
bool initialized = false;

const char* wave_filename = NULL;
static PyObject* wave_filename_obj = NULL;

static int
DoCallback(short* wave, espeak_EVENT_TYPE event, int pos, int len, int num_samples, const char* name)
{
	int isTrue = 1;
	if (wave != NULL && wave_filename != NULL) {
		FILE* output = fopen(wave_filename, "w+");
		fwrite(wave, num_samples*2, 1, output);
		fclose(output);
	}
	PyObject* result = PyObject_CallFunction(
		CallBack, const_cast<char *>("iiiis"), event, pos, len, num_samples, name);
	
	if (result != NULL) {
		isTrue = PyObject_IsTrue(result);
		Py_DECREF(result);
	}
	
	return isTrue;
}

int
PyEspeakCB(short* wave, int num_samples, espeak_EVENT* event)
{
	int count = 0;
	if (CallBack != NULL && event != NULL && Stopping == 0) {
		AsyncPython = 1;
		while (Stopping == 0) {
			count++;
			int isTrue = 1;
			PyGILState_STATE gs = PyGILState_Ensure();
			const char *name = NULL;
			if (event->type == espeakEVENT_MARK || event->type == espeakEVENT_PLAY) {
                          name = (event->id).name;
			} else if (event->type == espeakEVENT_SAMPLERATE){
			  char str[15];
			  sprintf(str, "%d", event->id.number);
			  name = str;
			}
			if (event->type != espeakEVENT_LIST_TERMINATED) {
				isTrue = DoCallback(NULL, event->type, event->text_position, event->length, 0, name);
			} else {
				isTrue = DoCallback(wave, event->type, event->text_position, event->length, num_samples, name);
			}
			PyGILState_Release(gs);
			if (!isTrue) {
				AsyncPython = 0;
				return 1; //abort
			}
			if (event->type == espeakEVENT_LIST_TERMINATED) break;
			++event;
		}
		AsyncPython = 0;
	}

	return Stopping;
}

static PyObject *
pyespeak_initialize(PyObject *self, PyObject *args, PyObject *kwdict) {
	initialized = true;
	int synchronous = 0;
	int playback = 1;
	int buffer_length = 400;
	espeak_AUDIO_OUTPUT espeak_mode;
	static const char *kwlist[] = {"synchronous", "playback", "buffer_length", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwdict, "|iii",
		 const_cast<char **>(kwlist),
		 &synchronous, &playback, &buffer_length))
	  return NULL;

	if (synchronous && playback) {
	  espeak_mode = AUDIO_OUTPUT_SYNCH_PLAYBACK;
	} else if (!synchronous && playback) {
	  espeak_mode = AUDIO_OUTPUT_PLAYBACK;
	} else if (synchronous && !playback) {
	  espeak_mode = AUDIO_OUTPUT_SYNCHRONOUS;
	} else {
	  espeak_mode = AUDIO_OUTPUT_RETRIEVAL;
	}
	bool result = espeak_Initialize(espeak_mode,
								    buffer_length,	// sound buffer length
									NULL,	// use default data directory
									0		// no flags
									);
	if (result) {
      espeak_SetSynthCallback(PyEspeakCB);
      Py_INCREF(Py_True);
      return Py_True;
    }
	Py_INCREF(Py_False);
	return Py_False;
}

static void
pyespeak_finalize() {
	// Segfaults sometimes. Not sure why. However, _not_ clearing this seems to
	// cause no obvious issue.
	// Py_CLEAR(CallBack);
	if (wave_filename_obj != NULL)
		Py_CLEAR(wave_filename_obj);
        if (initialized) espeak_Terminate();
}

static PyObject *
pyespeak_synth(PyObject *self, PyObject *args, PyObject *kwdict) {
	int r = EE_OK;
	const char *text;
	int start_pos = 0;
	int end_pos = 0;
	PyObject* enable_ssml = NULL;
	PyObject* enable_phonemes = NULL;
	PyObject* enable_endpause = NULL;
	PyObject* user_data = NULL;
	
	static const char *kwlist[] = {"text", "start_pos", "end_pos", "ssml", "phonemes", "endpause", "user_data", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwdict, "s|iiOOOO",
        const_cast<char **>(kwlist), &text, &start_pos, &end_pos,
        &enable_ssml, &enable_phonemes, &enable_endpause, &user_data))
		return NULL;
	
	int flags = 0;
	if (enable_ssml != NULL and PyObject_IsTrue(enable_ssml))
		flags |= espeakSSML;
	if (enable_phonemes != NULL and PyObject_IsTrue(enable_phonemes))
		flags |= espeakPHONEMES;
	if (enable_endpause != NULL and PyObject_IsTrue(enable_endpause))
		flags |= espeakENDPAUSE;
	
	size_t len = strlen(text) + 1;
	
	if (len > 0) {
		r = espeak_Synth(text, len, start_pos, POS_CHARACTER, end_pos,
			flags | espeakCHARS_AUTO, NULL, (void *) user_data);
		
		if (r == EE_BUFFER_FULL) {
			PyErr_SetString(BufferFullError, "command could not be buffered");
			return NULL;
		} else if(r == EE_INTERNAL_ERROR) {
			PyErr_SetString(PyExc_SystemError, "internal error within espeak");
			return NULL;
		} else {
			Py_INCREF(Py_True);
			return Py_True;
		}
	} else {
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject *
pyespeak_set_synth_callback(PyObject *self, PyObject *args)
{
	PyObject* cb;
	
	if (!PyArg_ParseTuple(args, "O", &cb)) {
		PyErr_SetString(BufferFullError, "invalid argument");
		return NULL;
	}

    //What??? Why can't there be more than one callback?
	if (CallBack != NULL)
		Py_CLEAR(CallBack);
	
	if(Py_None != cb) {
		if(!PyCallable_Check(cb)) {
			PyErr_SetString(BufferFullError, "not callable object");
			return NULL;
		}
		
		Py_INCREF(cb);
		CallBack = cb;
	}

    Py_RETURN_NONE;
}


static PyObject *
pyespeak_playing(PyObject *self, PyObject *args)
{
    if (espeak_IsPlaying()) {
	    Py_INCREF(Py_True);
	    return Py_True;
    } else {
	    Py_INCREF(Py_False);
	    return Py_False;
    }
}


static PyObject *
pyespeak_stop(PyObject *self, PyObject *args)
{
	Stopping = 1;
    // We need to release the GIL to avoid a deadlock with PyGILState_Ensure.
	PyThreadState* ts = PyEval_SaveThread();
    while (AsyncPython)
        usleep(100);
	espeak_Cancel();
	PyEval_RestoreThread(ts);
	Stopping = 0;
	
    Py_RETURN_NONE;
}

static PyObject *
pyespeak_set_voice(PyObject *self, PyObject *args, PyObject *kwdict) {
	espeak_VOICE voice;
	voice.name = NULL;
	voice.languages = NULL;
	voice.gender = 0;
	voice.age = 0;
	voice.variant = 0;

	static const char *kwlist[] = {"name", "language", "gender", "age",
		"variant", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwdict, "|ssiii",
        const_cast<char **>(kwlist), &voice.name, &voice.languages,
        &voice.gender, &voice.age, &voice.variant))
		return NULL;
	
	espeak_SetVoiceByProperties(&voice);
	
    Py_RETURN_NONE;
}

static PyObject *
pyespeak_set_parameter(PyObject *self, PyObject *args, PyObject *kwdict) {
	int parameter = 0;
	int value = 0;
	PyObject* isRelative = NULL;
	int relative;
	int r;
	
	static const char *kwlist[] = {"parameter", "value", "relative", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwdict, "ii|O",
        const_cast<char **>(kwlist), &parameter, &value, &isRelative))
		return NULL;
	
	relative = (isRelative == NULL) ? 0 : PyObject_IsTrue(isRelative);
	
	r = espeak_SetParameter((espeak_PARAMETER)parameter, value, relative);
	
	if (r == EE_BUFFER_FULL) {
		PyErr_SetString(BufferFullError, "command could not be buffered");
		return NULL;
	} else if(r == EE_INTERNAL_ERROR) {
		PyErr_SetString(PyExc_SystemError, "internal error within espeak");
		return NULL;
	} else {
		Py_INCREF(Py_True);
		return Py_True;
	}
}

static PyObject *
pyespeak_get_parameter(PyObject *self, PyObject *args) {
	int parameter = 0;
	int current = 0;
	PyObject* getCurrent;
	
	if (!PyArg_ParseTuple(args, "iO", &parameter, &getCurrent))
		return NULL;
	
	current = PyObject_IsTrue(getCurrent);
	
	return PyLong_FromLong(espeak_GetParameter((espeak_PARAMETER)parameter, current));
}

static PyObject *
pyespeak_list_voices(PyObject *self, PyObject *args) {
	const espeak_VOICE **voices_list = espeak_ListVoices(NULL);
	
	PyObject *python_list = PyList_New(0);
	for (int i = 0; voices_list[i]; i++) {
		const espeak_VOICE *item = voices_list[i];
		PyObject *this_list = Py_BuildValue("{s:s,s:s,s:s,s:i,s:i,s:s}",
			"name", item->name,
			"languages", item->languages,
			"identifier", item->identifier,
			"gender", item->gender,
			"age", item->age,
			"variant", NULL // only used in espeak_setVoiceByProperties
			);
		if (this_list == NULL) {
			PyErr_SetString(PyExc_SystemError, "Internal error creating voices list.");
			return NULL;
		}
		PyList_Append(python_list, this_list);
	}
	
	return python_list;
}


static PyObject*
pyespeak_set_wave_filename(PyObject *self, PyObject *args){
  // Reading args into "s" gives a char* that is garbage collected after this call.
  if (wave_filename_obj != NULL)
    Py_CLEAR(wave_filename_obj);
  if (!PyArg_ParseTuple(args, "s", &wave_filename)) {
    PyErr_SetString(BufferFullError, "invalid argument");
    return NULL;
  }
  wave_filename_obj = Py_BuildValue("s", wave_filename);
  Py_INCREF(wave_filename_obj);
  //printf("%s\n", wave_filename);
  Py_RETURN_NONE;
}

static PyObject* 
pyespeak_get_wave_filename(PyObject *self, PyObject *args){
  if (wave_filename_obj != NULL)
    return wave_filename_obj;
  Py_RETURN_NONE;
}

/* Module Methods Table */
static PyMethodDef EspeakMethods[] = {
	{"init", (PyCFunction)pyespeak_initialize, METH_VARARGS | METH_KEYWORDS,
		"Initialization. Should be called before any other function."},
	{"synth", (PyCFunction)pyespeak_synth, METH_VARARGS | METH_KEYWORDS,
		"Synthesizes the given text."},
	{"stop", pyespeak_stop, METH_VARARGS,
		"Stops speech synthesizer."},
	{"playing", pyespeak_playing, METH_VARARGS,
		"Returns True if speech synthesis is in progress."},
	{"set_synth_callback", pyespeak_set_synth_callback, METH_VARARGS,
		"Sets a sync callback."},
	{"set_voice", (PyCFunction)pyespeak_set_voice, METH_VARARGS | METH_KEYWORDS,
		"Change current voice characteristics."},
	{"set_parameter", (PyCFunction)pyespeak_set_parameter, METH_VARARGS | METH_KEYWORDS,
		"Changes a parameter, which may be one of: rate, volume, pitch, range, punctuation, capitals, wordgap."},
	{"get_parameter", pyespeak_get_parameter, METH_VARARGS,
		"Retrieves a parameter, which may be one of: rate, volume, pitch, range, punctuation, capitals, wordgap."},
	{"get_wave_filename", pyespeak_get_wave_filename, METH_VARARGS,
		"Get the name of the temporary file for sending wave content on callback."},
	{"set_wave_filename", pyespeak_set_wave_filename, METH_VARARGS,
		"Set the name of the temporary file for sending wave content on callback."},
	{"list_voices", pyespeak_list_voices, METH_VARARGS,
		"Lists all voices."},
	{NULL, NULL, 0, NULL}
};

// http://python3porting.com/cextensions.html
#if PY_MAJOR_VERSION >= 3
	#define MOD_ERROR_VAL NULL
	#define MOD_SUCCESS_VAL(val) val
	#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
	#define MOD_DEF(ob, name, doc, methods) \
			static struct PyModuleDef moduledef = { \
				PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
			ob = PyModule_Create(&moduledef);
#else
	#define MOD_ERROR_VAL
	#define MOD_SUCCESS_VAL(val)
	#define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
	#define MOD_DEF(ob, name, doc, methods) \
			ob = Py_InitModule(name, methods);
#endif

MOD_INIT(core) {
	// Initialize the Python module
	PyObject *module;
	
	PyEval_InitThreads();
	
	MOD_DEF(module, "core", NULL, EspeakMethods);
	
	if(module == NULL)
		return MOD_ERROR_VAL;
	
	// Add parameters
	PyModule_AddIntConstant(module, "parameter_RATE", espeakRATE);
	PyModule_AddIntConstant(module, "parameter_VOLUME", espeakVOLUME);
	PyModule_AddIntConstant(module, "parameter_PITCH", espeakPITCH);
	PyModule_AddIntConstant(module, "parameter_RANGE", espeakRANGE);
	PyModule_AddIntConstant(module, "parameter_PUNCTUATION", espeakPUNCTUATION);
	PyModule_AddIntConstant(module, "parameter_CAPITALS", espeakCAPITALS);
	PyModule_AddIntConstant(module, "parameter_WORDGAP", espeakWORDGAP);
	
	// Add event types
	PyModule_AddIntConstant(module, "event_LIST_TERMINATED", espeakEVENT_LIST_TERMINATED);
	PyModule_AddIntConstant(module, "event_WORD", espeakEVENT_WORD);
	PyModule_AddIntConstant(module, "event_SENTENCE", espeakEVENT_SENTENCE);
	PyModule_AddIntConstant(module, "event_MARK", espeakEVENT_MARK);
	PyModule_AddIntConstant(module, "event_PLAY", espeakEVENT_PLAY);
	PyModule_AddIntConstant(module, "event_END", espeakEVENT_END);
	PyModule_AddIntConstant(module, "event_MSG_TERMINATED", espeakEVENT_MSG_TERMINATED);
	PyModule_AddIntConstant(module, "event_PHONEME", espeakEVENT_PHONEME);
	PyModule_AddIntConstant(module, "event_SAMPLE_RATE", espeakEVENT_SAMPLERATE);
	// Add punctuation types
	PyModule_AddIntConstant(module, "punctuation_NONE", espeakPUNCT_NONE);
	PyModule_AddIntConstant(module, "punctuation_ALL", espeakPUNCT_ALL);
	PyModule_AddIntConstant(module, "punctuation_SOME", espeakPUNCT_SOME);
	
	// Create custom exceptions
	BufferFullError = PyErr_NewException(
		const_cast<char *>("espeak.BufferFullError"), NULL, NULL);
	Py_INCREF(BufferFullError);
	PyModule_AddObject(module, "error", BufferFullError);
	
	// Setup destructor
	atexit(pyespeak_finalize);

	return MOD_SUCCESS_VAL(module);
}
