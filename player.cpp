#include <stdio.h>
#include <assert.h>
#include <windows.h>
#include <mmsystem.h>

#include "mpg123.h"
#include "module.h"
#include "slotwait.h"
#include "slotsock.h"

static WAVEHDR WaveHdr[2];
static HWAVEOUT _ghWaveOut = NULL;
static char _outbuf[2][4410 * 4];

static int play_init(mpg123_handle *codecp, const char *path)
{
	int err;
	int channel, enc;

	long freq;
	size_t done;
	WAVEFORMATEX waveform;

	mpg123_param(codecp, MPG123_RESYNC_LIMIT, -1, 0);
	err = mpg123_open(codecp, (char *)path);
	if (err != MPG123_OK) {
		fprintf(stderr, "mpg123_open %d\n", err);
		return err;
	}

	err = mpg123_decode(codecp, NULL, 0, NULL, 0, &done);
	if (err != MPG123_NEW_FORMAT) {
		fprintf(stderr, "mpg123_decode %d\n", err);
		mpg123_close(codecp);
		return err;
	}

	err = mpg123_getformat(codecp, &freq, &channel, &enc);
	if (err != MPG123_OK) {
		fprintf(stderr, "mpg123_getformat %d\n", err);
		mpg123_close(codecp);
		return err;
	}

	waveform.wFormatTag		= WAVE_FORMAT_PCM;
	waveform.nChannels		= channel;
	waveform.nSamplesPerSec	= freq;
	waveform.nBlockAlign	= 16 * channel / 8;
	waveform.wBitsPerSample	= 16;
	waveform.cbSize			= 0;
	waveform.nAvgBytesPerSec = freq * channel * 16 / 8;

	err = waveOutOpen(&_ghWaveOut, WAVE_MAPPER,
		   	&waveform, (DWORD)slotwait_handle(), 0, CALLBACK_EVENT);

	memset(WaveHdr, 0, sizeof(WaveHdr));
	WaveHdr[0].dwLoops  = 1;
	WaveHdr[1].dwLoops  = 1;
	waveOutPrepareHeader(_ghWaveOut, &WaveHdr[0], sizeof(WAVEHDR));
	waveOutPrepareHeader(_ghWaveOut, &WaveHdr[1], sizeof(WAVEHDR));
	return err;
}

static int play_clean(mpg123_handle *codecp)
{
	waveOutReset(_ghWaveOut);
	waveOutUnprepareHeader(_ghWaveOut, &WaveHdr[0], sizeof(WaveHdr[0]));
	waveOutUnprepareHeader(_ghWaveOut, &WaveHdr[1], sizeof(WaveHdr[1]));
	waveOutClose(_ghWaveOut);
	mpg123_close(codecp);
	return 0;
}

static void waveout_scan(void *upp)
{
	int err;
	size_t done;
	LPWAVEHDR pWaveHdr;
	mpg123_handle *codecp;

	codecp = (mpg123_handle *)upp;
	assert(codecp != NULL);

	for (int i = 0; i < 2; i++) {
	   	pWaveHdr = &WaveHdr[i];
	   	if (pWaveHdr->dwFlags == 0x12)
			continue;
	   
		err = mpg123_decode(codecp, NULL, 0,
			   	(unsigned char *)_outbuf[i], sizeof(_outbuf[i]), &done);

		if (err == MPG123_ERR)
			break;

		pWaveHdr->lpData          = (LPTSTR)_outbuf[i];
	   	pWaveHdr->dwBufferLength  = done;

		err = waveOutWrite(_ghWaveOut, pWaveHdr, sizeof (WAVEHDR));
	}

	if (err == MPG123_ERR)
		printf("err = %s\n", mpg123_strerror(codecp));

	return;
}

static mpg123_handle *_codecp;
static struct waitcb _waveout_scan; 

static void module_init(void)
{
	int err;
	mpg123_init();
	_codecp = mpg123_new(NULL, &err);
	assert(_codecp != NULL);
	play_init(_codecp, "1.mp3");

	waitcb_init(&_waveout_scan, waveout_scan, _codecp);
	_waveout_scan.wt_flags &= ~WT_EXTERNAL;
	_waveout_scan.wt_flags |= WT_WAITSCAN;
	waitcb_switch(&_waveout_scan);
}

static void module_clean(void)
{
	play_clean(_codecp);
	mpg123_delete(_codecp);
	mpg123_exit();
}

struct module_stub player_mod = {
	module_init, module_clean
};

