#include "burner.h"
#include <xaudio2.h>
#include "cdsound.h"

WavClass::WavClass()
{
	//m_DirectSound		= 0;
	//m_primaryBuffer		= 0;
	//m_secondaryBuffer1	= 0;
}

WavClass::WavClass(const WavClass&)
{
	//
}

WavClass::~WavClass()
{
	//
}

bool WavClass::Initialize(HWND hwnd, TCHAR* szFile)
{
	bool result;

	// Initialize direct sound and the primary sound buffer.
	result = InitializeDirectSound(hwnd);
	if(!result)
	{
		return false;
	}

	// Load a wave audio file onto a secondary buffer.
	result = LoadWaveFile(szFile, NULL);
	if(!result)
	{
		return false;
	}

	return true;
}

void WavClass::Shutdown()
{
	// Release the secondary buffer.
	ShutdownWaveFile(NULL);

	// Shutdown the Direct Sound API.
	ShutdownDirectSound();

	return;
}

bool WavClass::InitializeDirectSound(HWND hwnd)
{
	return true;
}

void WavClass::ShutdownDirectSound()
{	 

	return;
}

bool WavClass::LoadWaveFile(TCHAR* filename, void** secondaryBuffer)
{
#if 0
	FILE* filePtr;
	unsigned int count;
	WaveHeaderType waveFileHeader;
	WAVEFORMATEX waveFormat;
	DSBUFFERDESC bufferDesc;
	HRESULT result;
	IDirectSoundBuffer* tempBuffer;
	unsigned char* waveData;
	unsigned char* bufferPtr;
	unsigned long bufferSize;


	// Open the wave file in binary.
	filePtr = _tfopen(filename, _T("rb"));
	if(!filePtr)
	{
		return false;
	}

	// Read in the wave file header.
	count = fread(&waveFileHeader, sizeof(waveFileHeader), 1, filePtr);
	if(count != 1)
	{
		return false;
	}

	// Check that the chunk ID is the RIFF format.
	if((waveFileHeader.chunkId[0] != 'R') || (waveFileHeader.chunkId[1] != 'I') || 
	   (waveFileHeader.chunkId[2] != 'F') || (waveFileHeader.chunkId[3] != 'F'))
	{
		return false;
	}

	// Check that the file format is the WAVE format.
	if((waveFileHeader.format[0] != 'W') || (waveFileHeader.format[1] != 'A') ||
	   (waveFileHeader.format[2] != 'V') || (waveFileHeader.format[3] != 'E'))
	{
		return false;
	}

	// Check that the sub chunk ID is the fmt format.
	if((waveFileHeader.subChunkId[0] != 'f') || (waveFileHeader.subChunkId[1] != 'm') ||
	   (waveFileHeader.subChunkId[2] != 't') || (waveFileHeader.subChunkId[3] != ' '))
	{
		return false;
	}

	// Check that the audio format is WAVE_FORMAT_PCM.
	if(waveFileHeader.audioFormat != WAVE_FORMAT_PCM)
	{
		return false;
	}

	// Check that the wave file was recorded in stereo format.
	if(waveFileHeader.numChannels != 2)
	{
		return false;
	}

	// Check that the wave file was recorded at a sample rate of 44.1 KHz.
	if(waveFileHeader.sampleRate != 44100)
	{
		return false;
	}

	// Ensure that the wave file was recorded in 16 bit format.
	if(waveFileHeader.bitsPerSample != 16)
	{
		return false;
	}

	// Check for the data chunk header.
	if((waveFileHeader.dataChunkId[0] != 'd') || (waveFileHeader.dataChunkId[1] != 'a') ||
	   (waveFileHeader.dataChunkId[2] != 't') || (waveFileHeader.dataChunkId[3] != 'a'))
	{
		return false;
	}

	// Set the wave format of secondary buffer that this wave file will be loaded onto.
	waveFormat.wFormatTag			= WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec		= 44100;
	waveFormat.wBitsPerSample		= 16;
	waveFormat.nChannels			= 2;
	waveFormat.nBlockAlign			= (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec		= waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize				= 0;

	GUID guidNULL;
	memset(&guidNULL,0,sizeof(GUID));

	// Set the buffer description of the secondary sound buffer that the wave file will be loaded onto.
	bufferDesc.dwSize			= sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags			= DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_CTRLFREQUENCY;
	bufferDesc.dwBufferBytes	= waveFileHeader.dataSize;
	bufferDesc.dwReserved		= 0;
	bufferDesc.lpwfxFormat		= &waveFormat;
	bufferDesc.guid3DAlgorithm	= guidNULL; //GUID_NULL;

	// Create a temporary sound buffer with the specific buffer settings.
	result = m_DirectSound->CreateSoundBuffer(&bufferDesc, &tempBuffer, NULL);
	if(FAILED(result))
	{
		return false;
	}

	// Test the buffer format against the direct sound 8 interface and create the secondary buffer.
	result = tempBuffer->QueryInterface(IID_IDirectSoundBuffer, (void**)&*secondaryBuffer);
	if(FAILED(result))
	{
		return false;
	}

	// Release the temporary buffer.
	tempBuffer->Release();
	tempBuffer = 0;

	// Move to the beginning of the wave data which starts at the end of the data chunk header.
	fseek(filePtr, sizeof(WaveHeaderType), SEEK_SET);

	// Create a temporary buffer to hold the wave file data.
	waveData = new unsigned char[waveFileHeader.dataSize];
	if(!waveData)
	{
		return false;
	}

	// Read in the wave file data into the newly created buffer.
	count = fread(waveData, 1, waveFileHeader.dataSize, filePtr);
	if(count != waveFileHeader.dataSize)
	{
		return false;
	}

	// Close the file once done reading.
	int error = fclose(filePtr);
	if(error != 0)
	{
		return false;
	}

	// Lock the secondary buffer to write wave data into it.
	result = (*secondaryBuffer)->Lock(0, waveFileHeader.dataSize, (void**)&bufferPtr, (DWORD*)&bufferSize, NULL, 0, 0);
	if(FAILED(result))
	{
		return false;
	}

	// Copy the wave data into the buffer.
	memcpy(bufferPtr, waveData, waveFileHeader.dataSize);

	// Unlock the secondary buffer after the data has been written to it.
	result = (*secondaryBuffer)->Unlock((void*)bufferPtr, bufferSize, NULL, 0);
	if(FAILED(result))
	{
		return false;
	}
	
	// Release the wave data since it was copied into the secondary buffer.
	delete [] waveData;
	waveData = 0;

#endif
	return true;
}

void WavClass::ShutdownWaveFile(void **)
{
	 
	return;
}

#include "cd_interface.h"

WavClass* wav;
extern HWND hScrnWnd;

void wav_exit()
{
	if(wav) {
		wav_stop();
		wav->Shutdown();
		wav = NULL;
	}
}

int wav_open(TCHAR* szFile)
{
	wav_exit();

	if(hScrnWnd) {
		wav = new WavClass;
		wav->Initialize(hScrnWnd, szFile);
	} else {
		return 0;
	}

	return 1;
}

void wav_stop() 
{
	if(!wav) return;

 
}

void wav_play()
{
	if(!wav) return;

	 
}

void wav_pause(bool bResume)
{
	if(!wav) return;	

	 
}
