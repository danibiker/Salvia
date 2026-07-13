/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#ifdef SAVE_RCSID
static char rcsid =
 "@(#) $Id: SDL_xboxaudio.c,v 1.1 2003/07/18 15:19:33 lantus Exp $";
#endif

/* Allow access to a raw mixing buffer */

#include <stdio.h>

#include "SDL_types.h"
#include "SDL_error.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "SDL_audio_c.h"
#include "SDL_xboxaudio.h"

/* Audio driver functions */
static int XboxDX_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void XboxDX_ThreadInit(_THIS);
static void XboxDX_WaitAudio_BusyWait(_THIS);
static void XboxDX_PlayAudio(_THIS);
static Uint8 *XboxDX_GetAudioBuf(_THIS);
static void XboxDX_WaitDone(_THIS);
static void XboxDX_CloseAudio(_THIS);

/* Audio driver bootstrap functions */

IXAudio2* g_sound;

static int Audio_Available(void)
{
	// Audio is always available on Xbox
	return(1);
}

/* Functions for loading the DirectX functions dynamically */
static HINSTANCE DSoundDLL = NULL;

static void XboxDX_Unload(void)
{
	// do nothing
}
static int XboxDX_Load(void)
{
	return 1;
}

static void Audio_DeleteDevice(SDL_AudioDevice *device)
{
	 if (device->hidden)
	 {
		 free(device->hidden);
		 device->hidden = NULL;
	 }
}

static SDL_AudioDevice *Audio_CreateDevice(int devindex)
{
	HRESULT result;
	SDL_AudioDevice *this;

	/* Load DirectX */
	if ( XboxDX_Load() < 0 ) {
		return(NULL);
	}

	/* Initialize all variables that we clean on shutdown */
	this = (SDL_AudioDevice *)malloc(sizeof(SDL_AudioDevice));
	if ( this ) {
		memset(this, 0, (sizeof *this));
		this->hidden = (struct SDL_PrivateAudioData *)
				malloc((sizeof *this->hidden));
	}
	if ( (this == NULL) || (this->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( this ) {
			free(this);
		}
		return(0);
	}
	memset(this->hidden, 0, (sizeof *this->hidden));

	/* Set the function pointers */
	this->OpenAudio = XboxDX_OpenAudio;
	this->ThreadInit = XboxDX_ThreadInit;
	this->WaitAudio = XboxDX_WaitAudio_BusyWait;
	this->PlayAudio = XboxDX_PlayAudio;
	this->GetAudioBuf = XboxDX_GetAudioBuf;
	this->WaitDone = XboxDX_WaitDone;
	this->CloseAudio = XboxDX_CloseAudio;

	this->free = Audio_DeleteDevice;

	// XAudio2 mezcla en el core 1 - thread 2
	result = XAudio2Create(&sound, 0, XboxThread5 );
	if ( result != S_OK ) {
		return(0);
	}

	/* Create a master voice */

	result = IXAudio2_CreateMasteringVoice(sound, &masterVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL );

	g_sound = sound;

	mixbuf = NULL;



	return this;
}

AudioBootStrap DSOUND_bootstrap = {
	"XAudio2", "XBOX 360 XAudio2 SDL Driver 0.01",
	Audio_Available, Audio_CreateDevice
};

/* DirectSound needs to be associated with a window */
static HWND mainwin = NULL;
/* */
void XboxDX_SoundFocus(int nHwnd)
{
	mainwin = (HWND)nHwnd;
}

static void XboxDX_ThreadInit(_THIS)
{
	XSetThreadProcessor(GetCurrentThread(), 5);
}

static void XboxDX_WaitAudio_BusyWait(_THIS)
{
	/* No-op: el ritmo de reproduccion lo marca XboxDX_PlayAudio, que espera
	   (cediendo la CPU con Sleep) a que XAudio2 libere un buffer antes de
	   encolar el siguiente. */

	if (!mixbuf)
		return;
}

static void XboxDX_PlayAudio(_THIS)
{
	XAUDIO2_VOICE_STATE state;
	XAUDIO2_BUFFER xa2buffer={0};

	while (1) {

		IXAudio2SourceVoice_GetState(mixbuf, &state, 1); //Wolf3s: "vs2010 Warning: Not actual parameter").
		xa2buffer.Flags = XAUDIO2_END_OF_STREAM;
         if (state.BuffersQueued < NUM_BUFFERS - 1) {
            if (state.BuffersQueued == 0) {
                            // buffers ran dry
            }
            // there is at least one free buffer
            break;
        }
        /* Todos los buffers encolados: cede HW5 en vez de girar al 100%.
           Con varios buffers encolados (decenas de ms) dormir 1 ms no
           provoca underrun y libera el core para su hermano SMT (HW4). */
        Sleep(1);
    }

	if (locked_buf)
	{

		memcpy(&pAudioBuffers[currentBuffer * mixlen], locked_buf, mixlen);
		xa2buffer.AudioBytes=mixlen;
		xa2buffer.pAudioData= &pAudioBuffers[currentBuffer * mixlen];
		xa2buffer.pContext=NULL;

		IXAudio2SourceVoice_SubmitSourceBuffer(mixbuf, &xa2buffer, NULL);

		currentBuffer++;
		currentBuffer %= (NUM_BUFFERS);

	}


}

static Uint8 *XboxDX_GetAudioBuf(_THIS)
{

	return(locked_buf);
}

static void XboxDX_WaitDone(_THIS)
{
	Uint8 *stream;

	/* Wait for the playing chunk to finish */
	stream = this->GetAudioBuf(this);
	if ( stream != NULL ) {
	memset(stream, silence, mixlen);
	this->PlayAudio(this);
	}
	this->WaitAudio(this);

	if (!mixbuf)
		return;

	/* Stop the looping sound buffer */

}

static void XboxDX_CloseAudio(_THIS)
{
	if (mixbuf) {
        IXAudio2SourceVoice_Stop(mixbuf, 0, XAUDIO2_COMMIT_NOW);
        IXAudio2SourceVoice_DestroyVoice(mixbuf);
        mixbuf = NULL;
    }
    if (masterVoice) {
        IXAudio2MasteringVoice_DestroyVoice(masterVoice);
        masterVoice = NULL;
    }
    if (sound) {
        IXAudio2_Release(sound);
        sound = NULL;
        g_sound = NULL;
    }
    if (pAudioBuffers) {
        free(pAudioBuffers);
        pAudioBuffers = NULL;
    }
    if (locked_buf) {
        free(locked_buf);
        locked_buf = NULL;
    }
}

/* This function tries to create a secondary audio buffer, and returns the
   number of audio chunks available in the created buffer.
*/



static int CreateSecondary(IXAudio2 *sndObj, int focus,
	IXAudio2SourceVoice *sndbuf, WAVEFORMATEX *wavefmt, Uint32 chunksize)
{
	const int numchunks = 2;


	IXAudio2_CreateSourceVoice(sndObj, &sndbuf, wavefmt, XAUDIO2_VOICE_USEFILTER ,
								XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL );


	return(numchunks);
}

static int XboxDX_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
	int nXAudio2Fps = 60*100;
	int nAudSegLen = (spec->freq * 100 + (nXAudio2Fps >> 1)) / nXAudio2Fps;
	int nAudSegCount = 6;
    int nAudAllocSegLen = nAudSegLen << 2;
    int cbLoopLen = (nAudSegLen * nAudSegCount) << 2;
	HRESULT hr;

	WAVEFORMATEX waveformat;

	mixbuf = NULL;



	/* Set basic WAVE format parameters */
	memset(&waveformat, 0, sizeof(waveformat));
	waveformat.wFormatTag = WAVE_FORMAT_PCM;

	/* Determine the audio parameters from the AudioSpec */
	switch ( spec->format & 0xFF ) {
		case 8:
			/* Unsigned 8 bit audio data */
			spec->format = AUDIO_U8;
			silence = 0x80;
			waveformat.wBitsPerSample = 8;
			break;
		case 16:
			/* Signed 16 bit audio data */
			spec->format = AUDIO_S16;
			silence = 0x00;
			waveformat.wBitsPerSample = 16;
			break;
		default:
			SDL_SetError("Unsupported audio format");
			return(-1);
	}
	waveformat.nChannels = spec->channels;
	waveformat.nSamplesPerSec = spec->freq;
	waveformat.nBlockAlign =
		waveformat.nChannels * (waveformat.wBitsPerSample/8);
	waveformat.nAvgBytesPerSec =
		waveformat.nSamplesPerSec * waveformat.nBlockAlign;

	/* Update the fragment size as size in bytes */
	SDL_CalculateAudioSpec(spec);

	locked_buf = (BYTE *)malloc(spec->size);

	/* Create the audio buffer to which we write (XAudio2 source voice) */
	NUM_BUFFERS = 4;

	pAudioBuffers = (BYTE *)malloc(spec->size*NUM_BUFFERS);

	hr = IXAudio2_CreateSourceVoice(sound, &mixbuf, &waveformat,
                                         XAUDIO2_VOICE_USEFILTER,
                                         XAUDIO2_DEFAULT_FREQ_RATIO,
                                         NULL, NULL, NULL);

	if (FAILED(hr) || mixbuf == NULL) {
		SDL_SetError("XboxDX: IXAudio2_CreateSourceVoice failed: 0x%08X", hr);
		return -1;
	}

	IXAudio2SourceVoice_Start(mixbuf, 0, 0);

	/* The buffer will auto-start playing in DX5_WaitAudio() */
	playing = 0;
	mixlen = spec->size;

	return(0);
}
