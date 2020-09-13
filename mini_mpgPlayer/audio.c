#include "audio.h"
#include <stdio.h>
#include <Windows.h>

#pragma comment(lib, "Winmm.lib")

static CRITICAL_SECTION g_cs;
static HWAVEOUT g_WaveDev;
static uint32_t g_nBlocks;

static void CALLBACK waveout_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	if (uMsg == WOM_DONE) {
		LPWAVEHDR wh = (LPWAVEHDR)dwParam1;
		waveOutUnprepareHeader(hwo, wh, sizeof(WAVEHDR));
		HGDIOBJ hg = GlobalHandle(wh->lpData);
		if(hg && GlobalUnlock(hg))
			GlobalFree(hg);
		hg = GlobalHandle(wh);
		if (hg && GlobalUnlock(hg))
			GlobalFree(hg);
		EnterCriticalSection(&g_cs);
		--g_nBlocks;
		LeaveCriticalSection(&g_cs);
	}
}

int audio_open(uint32_t rate)
{
	if (!waveOutGetNumDevs()) {
		return -1;
	}

	WAVEFORMATEX wfx;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.wBitsPerSample = 16;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = rate;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * 4;
	wfx.nBlockAlign = 4;

	if (waveOutOpen(&g_WaveDev, WAVE_MAPPER, &wfx, (DWORD_PTR)waveout_callback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
		return -1;
	}

	waveOutReset(g_WaveDev);
	InitializeCriticalSection(&g_cs);

	return 0;
}

void audio_close(void)
{
	if (g_WaveDev) {
		while (g_nBlocks)
			Sleep(76);

		waveOutReset(g_WaveDev);
		waveOutClose(g_WaveDev);
		g_WaveDev = NULL;
		DeleteCriticalSection(&g_cs);
	}
}

int play_samples(const void* data, uint32_t len)
{
	while (g_nBlocks >> 4)
		Sleep(76);

	HGDIOBJ hg_Data = GlobalAlloc(GMEM_MOVEABLE, len);
	if (!hg_Data) {
		return -1;
	}
	void* pData = GlobalLock(hg_Data);
	if (!pData) {
		GlobalFree(hg_Data);
		return -1;
	}
	CopyMemory(pData, data, len);

	HGDIOBJ hg_Hdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(WAVEHDR));
	if (!hg_Hdr) {
		GlobalUnlock(hg_Data);
		GlobalFree(hg_Data);
		return -1;
	}
	LPWAVEHDR wh = GlobalLock(hg_Hdr);
	if (!wh) {
		GlobalUnlock(hg_Data);
		GlobalFree(hg_Data);
		GlobalFree(hg_Hdr);
		return -1;
	}
	wh->dwBufferLength = len;
	wh->lpData = pData;

	do {
		if (waveOutPrepareHeader(g_WaveDev, wh, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			break;
		if (waveOutWrite(g_WaveDev, wh, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
			waveOutUnprepareHeader(g_WaveDev, wh, sizeof(WAVEHDR));
			break;
		}
		EnterCriticalSection(&g_cs);
		++g_nBlocks;
		LeaveCriticalSection(&g_cs);
		return 0;
	} while (0);

	GlobalUnlock(hg_Data);
	GlobalFree(hg_Data);
	GlobalUnlock(hg_Hdr);
	GlobalFree(hg_Hdr);

	return -1;
}
