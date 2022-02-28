/*
Playback code base retrieved from https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm__min_8c-example.html on 28th February 2022
Hardware parameter code base retrieved from https://www.linuxjournal.com/article/6735 on 28th February 2022
*/

#include <alsa/asoundlib.h>

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

static char *device = "hw:1,0";          /* USB playback device */
//static char *device = "hw:2,0";        /* HDMI 1 playback device */
//static char *device = "hw:0,0";        /* HDMI 0 playback device */

snd_output_t *output = NULL;
snd_pcm_format_t formatType;
snd_pcm_access_t accessType;
unsigned int channels = 1;
unsigned int samplingRate = 48000;
int soft_resample = 0;
unsigned int pcmLatency = 0;
unsigned char buffer[16*1024];  /* some random data */

/* Display information about the PCM interface */
void getHardwareParameters() {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val, val2;
    int dir;
    snd_pcm_uframes_t frames;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, device,
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

    /* One channels (mono) */
    snd_pcm_hw_params_set_channels(handle, params, channels);

    /* 44100 bits/second sampling rate (CD quality) */
    snd_pcm_hw_params_set_rate_near(handle,
            params, &samplingRate, &dir);

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

    printf("\n\n format type:%d\n", formatType);
    printf("\n\n access type:%d\n", accessType);
}

void sendSignalViaALSA() {
    int err;
        unsigned int i;
        snd_pcm_t *handle;
        snd_pcm_sframes_t frames;

        for (i = 0; i < sizeof(buffer); i++)
                buffer[i] = random() & 0xff;

        if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }


        if ((err = snd_pcm_set_params(handle,
                                      formatType,
                                      accessType,
                                      channels,
                                      samplingRate,
                                      soft_resample,
                                      pcmLatency)) < 0) {
                printf("Playback open error: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
        }

        for (i = 0; i < 16; i++) {
                frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
                if (frames < 0)
                        frames = snd_pcm_recover(handle, frames, 0);
                if (frames < 0) {
                        printf("snd_pcm_writei failed: %s\n", snd_strerror(err));
                        break;
                }
                if (frames > 0 && frames < (long)sizeof(buffer))
                        printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
        }

        snd_pcm_close(handle);
}

int main(void) {
    getHardwareParameters();
    sendSignalViaALSA();
}