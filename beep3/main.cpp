// http://thenerdcompany.blogspot.com/2014/02/sound-beep-on-raspberry-pi-using-c-and.html

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <math.h>
#include <time.h>
#include <chrono>
#include <unistd.h>
#include <iostream>
#define BUFFER_LEN 320

static char *device = "default:0";                       //soundcard
snd_output_t *output = NULL;
float buffer [BUFFER_LEN];
float buffer2 [BUFFER_LEN];

using namespace std::chrono;
using namespace std;

uint64_t getCurTime_microseconds(int clk_id)
{
	//struct timeval gettime_now;
	//gettimeofday(&gettime_now, NULL);

	//return (uint64_t) gettime_now.tv_sec * 1000000l + (uint64_t) gettime_now.tv_usec;
    //

    //std::chrono::high_resolution_clock::now();
    system_clock::now();
}

// Get time stamp in microseconds.
// https://stackoverflow.com/questions/21856025/getting-an-accurate-execution-time-in-c-micro-seconds
uint64_t micros()
{
    //time_point<microseconds> cur_time = system_clock::now().time_since_epoch();
    uint64_t us = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
    //printf("%d\n", us);
    return us; 
}

int main(void)
{
    uint64_t time_start = micros();
    uint64_t time_play;
    uint64_t time_play2;
    uint64_t time_stop;
    uint64_t time_stop2;
    int err;
    int j,k;

    int f = 880;                //frequency
    int fs = 48000;             //sampling frequency

    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;


    // ERROR HANDLING

    cout << 1 << endl;
    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    cout << 2 << endl;
    if ((err = snd_pcm_set_params(handle,
                    SND_PCM_FORMAT_FLOAT,
                    SND_PCM_ACCESS_RW_NONINTERLEAVED,
                    1,
                    48000,
                    1,
                    0)) < 0) {   
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);


    }
    cout << 3 << endl;

    // SINE WAVE
    //printf("Sine tone at %dHz ",f);

    for (k=0; k<BUFFER_LEN; k++){

        //buffer[k] = (sin(2*M_PI*f/fs*k));                 //sine wave value generation                        
        buffer[k] = k % 2; // AS: square wave it is
        buffer2[k] = k % 2; // AS: square wave it is
    }

    float* buffers[2] = {buffer, buffer2};

    time_play = micros();
    for (j=0; j<5; j++){
        frames = snd_pcm_writen(handle, (void**)buffers, BUFFER_LEN);    //sending values to sound driver
    }
    time_stop = micros();
    cout << 4 << endl;
    //snd_pcm_drain(handle);
    //usleep(1000000);
    time_play2 = micros();
    //for (j=0; j<5; j++){
    //    frames = snd_pcm_writei(handle, buffer, BUFFER_LEN);    //sending values to sound driver
    //}
    time_stop2 = micros();

    for(int i = 0; i < 10000000; i++)
    {
	    int tmp = 5 * i;
    }


    cout << "total: " << time_stop - time_start << endl;
    cout << "play:  " << time_stop - time_play << endl;
    cout << "play2: " << time_stop2 - time_play2 << endl;


    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;

}
