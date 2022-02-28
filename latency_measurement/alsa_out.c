/*
Code base retrieved from https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm__min_8c-example.html on 28th February 2022
*/

#include <alsa/asoundlib.h>

static char *device = "hw:1,0";          /* USB playback device */
//static char *device = "hw:2,0";        /* HDMI 1 playback device */
//static char *device = "hw:0,0";        /* HDMI 0 playback device */

snd_output_t *output = NULL;
unsigned char buffer[16*1024];                          /* some random data */

void displayHardwareParameters() {
    /* Display information about the PCM interface */

    printf("PCM handle name = '%s'\n",
            snd_pcm_name(handle));

    printf("PCM state = %s\n",
            snd_pcm_state_name(snd_pcm_state(handle)));

    snd_pcm_hw_params_get_access(params,
            (snd_pcm_access_t *) &val);
    printf("access type = %s\n",
            snd_pcm_access_name((snd_pcm_access_t)val));

    snd_pcm_hw_params_get_format(params, &val);
    printf("format = '%s' (%s)\n",
            snd_pcm_format_name((snd_pcm_format_t)val),
            snd_pcm_format_description(
                (snd_pcm_format_t)val));

    snd_pcm_hw_params_get_subformat(params,
            (snd_pcm_subformat_t *)&val);
    printf("subformat = '%s' (%s)\n",
            snd_pcm_subformat_name((snd_pcm_subformat_t)val),
            snd_pcm_subformat_description(
                (snd_pcm_subformat_t)val));

    snd_pcm_hw_params_get_channels(params, &val);
    printf("channels = %d\n", val);

    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("rate = %d bps\n", val);

    snd_pcm_hw_params_get_period_time(params,
            &val, &dir);
    printf("period time = %d us\n", val);

    snd_pcm_hw_params_get_period_size(params,
            &frames, &dir);
    printf("period size = %d frames\n", (int)frames);

    snd_pcm_hw_params_get_buffer_time(params,
            &val, &dir);
    printf("buffer time = %d us\n", val);

    snd_pcm_hw_params_get_buffer_size(params,
            (snd_pcm_uframes_t *) &val);
    printf("buffer size = %d frames\n", val);
}

int main(void) {
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

        displayHardwareParameters();

        if ((err = snd_pcm_set_params(handle,
                                      SND_PCM_FORMAT_U8,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      1,
                                      48000,
                                      1,
                                      500000)) < 0) {   /* 0.5sec */
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
        return 0;
}