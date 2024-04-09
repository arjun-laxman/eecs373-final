#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portaudio.h"

#define SAMPLE_RATE 44100
#define DURATION 1
#define FREQ_MIDDLE_C 222.0

// function to generate tone depending on Note, Amplitude, and Instrument
short generateTone(double fundamentalFreq, int duration, double harmonics[], int numHarmonics)
{
    int numSamples = duration * SAMPLE_RATE;
    for (int i = 0; i < numSamples; i++)
    {
        double sample = 0;
        double t = (double)i / SAMPLE_RATE;
        // Sum harmonics
        for (int h = 0; h < numHarmonics; h++)
        {
            sample += sin(2 * M_PI * (fundamentalFreq * (h + 1)) * t) * harmonics[h];
        }
        return (short)(sample * 32767);
        // fwrite(&amplified, sizeof(short), 1, file);
    }
}

// Callback function for PortAudio to generate audio
static int playToneCallback(const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo *timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            double* harmonics)
{
    float *out = (float *)outputBuffer;
    static double t = 0; // time variable

    for (unsigned long i = 0; i < framesPerBuffer; i++)
    {
        double sample = 0;
        // Example with a single harmonic, modify as needed
        // sample = sin(2 * M_PI * FREQ_MIDDLE_C * t) * harmonics[0]; // Simplified
        sample = generateTone(FREQ_MIDDLE_C, DURATION, harmonics, sizeof(harmonics) / sizeof(double));

        *out++ = sample; // Assuming mono output
        t += 1.0 / SAMPLE_RATE;
    }

    return paContinue;
}

int main()
{
    PaStream *stream;
    PaError err;
    double harmonics[] = {1.0}; // User data, adjust as necessary
    double pianoHarmonics[] = {1.0, 0.75, 0.5, 0.25}; // Simplified amplitude ratios for harmonics
    double fluteHarmonics[] = {1.0, 0.1, 0.05};       // Flute has fewer, softer harmonics

    err = Pa_Initialize();
    if (err != paNoError)
        goto error;

    err = Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, SAMPLE_RATE, 256, playToneCallback, harmonics);
    if (err != paNoError)
        goto error;

    err = Pa_StartStream(stream);
    if (err != paNoError)
        goto error;

    // Play for a duration, then close
    Pa_Sleep(DURATION * 1000);

    err = Pa_StopStream(stream);
    if (err != paNoError)
        goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError)
        goto error;

    Pa_Terminate();
    printf("Playback finished.\n");
    return 0;

error:
    Pa_Terminate();
    fprintf(stderr, "An error occurred while using the PortAudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
    return -1;
}
