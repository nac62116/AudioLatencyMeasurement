/*
Playback code base retrieved from https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm__min_8c-example.html on 28th February 2022
Hardware parameter code base retrieved from https://www.linuxjournal.com/article/6735 on 28th February 2022
*/

#include <alsa/asoundlib.h>

char *alsaPcmDevice = "hw:CARD=usb_audio_top";          /* USB playback device */

void sendSignalViaALSA() {
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw_params;
    short samples[48000];
    int bufferSize = sizeof(samples) / sizeof(samples[0]);

    // Initialize the samples somehow
    for (int byte = 0; byte < bufferSize; byte++) {
        samples[byte] = random() & 0xff;
    }

    snd_pcm_open(&pcm, alsaPcmDevice, SND_PCM_STREAM_PLAYBACK, 0);

    snd_pcm_hw_params_alloca(&hw_params);

    snd_pcm_hw_params_any(pcm, hw_params);
    snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hw_params, 1);
    snd_pcm_hw_params_set_rate(pcm, hw_params, 48000, 0);
    snd_pcm_hw_params_set_periods(pcm, hw_params, 10, 0);
    snd_pcm_hw_params_set_period_time(pcm, hw_params, 100000, 0); // 0.1 seconds

    snd_pcm_hw_params(pcm, hw_params);

    snd_pcm_writei(pcm, samples, 48000);

    snd_pcm_drain(pcm);
    snd_pcm_close(pcm);
}

int main(void) {
    sendSignalViaALSA();
}