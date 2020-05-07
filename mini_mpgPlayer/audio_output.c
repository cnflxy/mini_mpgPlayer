#include "audio_output.h"
//#include <stdio.h>
#include <Windows.h>

#pragma comment(lib, "Winmm.lib")

static CRITICAL_SECTION g_cs;

static HWAVEOUT g_WaveDev;
static int g_nBslocks;

static void CALLBACK waveout_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WOM_DONE) {
		EnterCriticalSection(&g_cs);
		LPWAVEHDR wh = (LPWAVEHDR)dwParam1;
		waveOutUnprepareHeader(hwo, wh, sizeof(WAVEHDR));
		HGDIOBJ hg = GlobalHandle(wh->lpData);
		GlobalUnlock(hg);
		GlobalFree(hg);
		hg = GlobalHandle(wh);
		GlobalUnlock(hg);
		GlobalFree(hg);
		--g_nBslocks;
		LeaveCriticalSection(&g_cs);
	}
}

bool audio_open(unsigned short nch, unsigned rate)
{
	if (!waveOutGetNumDevs()) {
		MessageBoxA(GetConsoleWindow(), "no audio devices!", "", MB_OK);
		return false;
	}

	WAVEFORMATEX wfx;
	MMRESULT mmr;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.wBitsPerSample = 16;
	wfx.nChannels = nch;
	wfx.nSamplesPerSec = rate;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nBlockAlign = wfx.nAvgBytesPerSec / wfx.nSamplesPerSec;	// wfx.nChannels * wfx.wBitsPerSample / 8

	if ((mmr = waveOutOpen(&g_WaveDev, NULL, &wfx, (DWORD_PTR)waveout_callback, 0, CALLBACK_FUNCTION)) != MMSYSERR_NOERROR) {
		MessageBoxA(GetConsoleWindow(), "open audio device failed!", "", MB_OK);
		return false;
	}

	waveOutReset(g_WaveDev);
	InitializeCriticalSection(&g_cs);
	g_nBslocks = 0;
	return true;
}

void audio_close(void)
{
	if (g_WaveDev) {
		while (g_nBslocks > 0)
			Sleep(77);

		waveOutReset(g_WaveDev);
		waveOutClose(g_WaveDev);
		g_WaveDev = NULL;
		DeleteCriticalSection(&g_cs);
		g_nBslocks = 0;
	}
}

bool play_samples(const void* data, unsigned len)
{
	while (g_nBslocks > 8)
		Sleep(77);

	HGDIOBJ hg_Data = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!hg_Data) {
		MessageBoxA(GetConsoleWindow(), "GlobalAlloc failed!(hg_Data)", "Error...", MB_OK);
		return false;
	}
	void* pData = GlobalLock(hg_Data);
	CopyMemory(pData, data, len);

	HGDIOBJ hg_Hdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(WAVEHDR));
	if (!hg_Hdr) {
		GlobalUnlock(hg_Data);
		GlobalFree(hg_Data);
		MessageBoxA(GetConsoleWindow(), "GlobalAlloc failed!(hg_Hdr)", "Error...", MB_OK);
		return false;
	}
	LPWAVEHDR wh = GlobalLock(hg_Hdr);
	wh->dwBufferLength = len;
	wh->lpData = pData;

	EnterCriticalSection(&g_cs);

	do {
		if (waveOutPrepareHeader(g_WaveDev, wh, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			break;
		if (waveOutWrite(g_WaveDev, wh, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
			waveOutUnprepareHeader(g_WaveDev, wh, sizeof(WAVEHDR));
			break;
		}
		++g_nBslocks;
		LeaveCriticalSection(&g_cs);
		return true;
	} while (false);

	GlobalUnlock(hg_Data);
	GlobalFree(hg_Data);
	GlobalUnlock(hg_Hdr);
	GlobalFree(hg_Hdr);
	LeaveCriticalSection(&g_cs);

	return false;
}
