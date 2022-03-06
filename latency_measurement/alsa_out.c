/*
Playback code base retrieved from https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm__min_8c-example.html on 28th February 2022
Hardware parameter code base retrieved from https://www.linuxjournal.com/article/6735 on 28th February 2022
*/

#include <alsa/asoundlib.h>
#include <stdio.h>

// BUFFER_SIZE = ALSA_PCM_PREFERRED_SAMPLE_RATE (48000 kHz) * SIGNAL_LENGTH_IN_S (0.001 s)
#define BUFFER_SIZE 32

const double SIGNAL_LENGTH_IN_S = 0.001;
const int ALSA_PCM_SOFT_RESAMPLE = 0;
const unsigned int ALSA_PCM_LATENCY = 0;
const unsigned int ALSA_PCM_PREFERRED_SAMPLE_RATE = 48000;

char *alsaPcmDevice = "hw:CARD=usb_audio_top";          /* USB playback device */
//const char *device = "hw:2,0";        /* HDMI 1 playback device */
//const char *device = "hw:0,0";        /* HDMI 0 playback device */
snd_pcm_format_t formatType;
snd_pcm_access_t accessType;
snd_pcm_uframes_t periodSize;
snd_pcm_uframes_t bufferSize;
unsigned int channels;
unsigned int sampleRate = ALSA_PCM_PREFERRED_SAMPLE_RATE;
unsigned char buffer[BUFFER_SIZE];

/* Get information about the PCM interface */
void getHardwareParameters() {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, alsaPcmDevice,
            SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr,
                "unable to open pcm device: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params,
            SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, channels);

    /* 48000 bits/second sampling rate */
    snd_pcm_hw_params_set_rate_near(handle, params, &sampleRate, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr,
                "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Get information about the PCM interface */

    snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
    accessType = (snd_pcm_access_t) val;

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *) &val);
    formatType = (snd_pcm_format_t) val;

    snd_pcm_hw_params_get_channels(params, &val);
    channels = val;

    snd_pcm_hw_params_get_rate(params, &val, &dir);
    sampleRate = val;

    snd_pcm_hw_params_get_period_size_min(params, (snd_pcm_uframes_t *) &val, &dir);
    periodSize = (snd_pcm_uframes_t) val;

    snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);
    bufferSize = (snd_pcm_uframes_t) val;

    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    printf("period time = %d us\n", val);

    printf("\n\n format type:%s\n", snd_pcm_format_name((snd_pcm_format_t) accessType));
    printf("\n\n access type:%s\n", snd_pcm_access_name((snd_pcm_access_t) accessType));
    printf("\n\n channels:%d\n", channels);
    printf("\n\n sample rate:%d\n\n", sampleRate);
    printf("\n\n buffer size: %ld\n\n", bufferSize);
    printf("\n\n period size: %ld\n\n", periodSize);

    snd_pcm_close(handle);
}

void sendSignalViaALSA() {
    int err;
        unsigned int i;
        snd_pcm_t *handle;
        snd_pcm_sframes_t frames;
        //int bufferSize = sizeof(buffer) / sizeof(buffer[0]);

        for (i = 0; i < sizeof(buffer); i++) {
            buffer[i] = random() & 0xff;
        }

        if ((err = snd_pcm_open(&handle, alsaPcmDevice, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }

        if ((err = snd_pcm_set_params(handle,
                                      formatType,
                                      accessType,
                                      channels,
                                      sampleRate,
                                      ALSA_PCM_SOFT_RESAMPLE,
                                      ALSA_PCM_LATENCY)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }

        for (i = 0; i < 10; i++) {
                for (int j = 0; j < 150; j++) {
                        frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
                        if (frames < 0)
                                frames = snd_pcm_recover(handle, frames, 0);
                        if (frames < 0) {
                                printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
                                //break;
                        }
                        if (frames > 0 && frames < (long) sizeof(buffer)) {
                                printf("Short write (expected %li, wrote %li)\n", (long) sizeof(buffer), frames);
                        }
                }
                sleep(1);
        }

        snd_pcm_close(handle);
}

int main(void) {
    getHardwareParameters();
    //sendSignalViaALSA();
}