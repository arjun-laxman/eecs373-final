#include <stdint.h>

#ifndef AUDIO_H
#define AUDIO_H

#define MAX_NOTES 16


typedef struct audio_ctx_s {
	int num_notes;
	int notes[MAX_NOTES];
	int cycles[MAX_NOTES];
	int cycles_per_wave[MAX_NOTES];
	float amps[MAX_NOTES];
	uint16_t damp_factor; // High or low bits for high or low damp
} audio_ctx_t;

void init_audio_ctx();

/*
 * Fills in list of frequencies freqs;
 */
void fill_freqs();

/*
 * Adds note to list of notes.
 */
void add_note(int note_idx);

/*
 * Sets the damp factor for a given note (existing in audio ctx)
 * If high is 1, sets it to high, else low damping factor
 */
void set_damp_factor(int note, int high);

/*
 * Decays all amplitudes by some factor and removes dead notes.
 */
void update_amps();

/*
 * Inits the timer
 */
void init_timer();

void audio_tim_isr();

#endif
