#include <stdint.h>
#include "display.h"

#ifndef AUDIO_H
#define AUDIO_H

#define MAX_NOTES 48
#define NUM_MODES 4

#define NUM_TUT_NOTES 3

#define TUT_X 5
#define TUT_Y 10

#define CORR_X (DISP_WIDTH - (2*CHAR_WIDTH + CHAR_PADDING) * 10)/2
#define CORR_Y (DISP_HEIGHT - CHAR_HEIGHT * 10)/4

typedef struct audio_ctx_s {
	int num_notes;
	int notes[MAX_NOTES];
	int cycles[MAX_NOTES];
	int cycles_per_wave[MAX_NOTES];
	float amps[MAX_NOTES];
	float played_amps[MAX_NOTES];
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
void add_note(int note_idx, float note_amp);

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

void print_mode();

void tutorial_check_note(uint8_t note_idx);

void handle_note_correct(uint8_t note_idx);

void handle_note_incorrect(uint8_t note_idx);

uint8_t is_note_correct(uint8_t note_idx);

void tut_init_display();

#endif
