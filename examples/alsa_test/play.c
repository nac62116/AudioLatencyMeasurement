// source: https://www.linuxjournal.com/article/6735
/*

   This example reads standard from input and writes
   to the default PCM device for 5 seconds of data.

*/

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdio.h>

int main() {
    long loops;
    int rc;
    int size;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;
    snd_pcm_uframes_t frames;
    char *buffer;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "hw:CARD=usb_audio_top",
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
    snd_pcm_hw_params_set_channels(handle, params, 2);

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params,
            &val, &dir);

    /* Set period size to 32 frames. */
    snd_pcm_hw_params_get_period_size_min(params, (snd_pcm_uframes_t *) &frames, &dir);
    //frames = (snd_pcm_uframes_t) frames;
    //frames = 32;
    snd_pcm_hw_params_set_period_size_near(handle,
            params, &frames, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr,
                "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames,
            &dir);
    printf("frames: %ld", frames);
    size = frames * 4; /* 2 bytes/sample, 2 channels */
    buffer = (char *) malloc(size);

    for (int byte = 0; byte < size; byte++) {
        buffer[byte] = (byte % 2) & 0xff;
    }

    /* We want to loop for 5 seconds */
    //snd_pcm_hw_params_get_period_time(params, &val, &dir);
    

    for (int i = 0; i < 10; i++) {
        /* 5 seconds in microseconds divided by
        * period time */
        loops = 0.001 * 1000000 / val;
        if (loops == 0) {
            loops = 1;
        }
        printf("loops: %d\n", loops);
        while (loops > 0) {
            rc = snd_pcm_writei(handle, buffer, frames);
            if (rc == -EPIPE) {
                /* EPIPE means underrun */
                fprintf(stderr, "underrun occurred\n");
                snd_pcm_prepare(handle);
            } else if (rc < 0) {
                fprintf(stderr,
                        "error from writei: %s\n",
                        snd_strerror(rc));
            }  else if (rc != (int)frames) {
                fprintf(stderr,
                        "short write, write %d frames\n", rc);
            }
            loops--;
        }
        sleep(1);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    return 0;
}
