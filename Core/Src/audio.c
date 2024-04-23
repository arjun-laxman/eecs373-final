#include <stdint.h>
#include <string.h>
#include <math.h>
#include "stm32l4xx_hal.h"
#include "audio.h"
#include "display.h"

#define LUT_SIZE 256
#define INIT_AMP 0.5
#define DEAD_THRESHOLD 0.0001
#define NUM_HARM 6

#define PRESCALER 1
#define BASE_CLK 120000000
#define LOWEST_FREQ 55

#define PI 3.14159265
#define HIGH_DAMP_FACTOR 0.95
#define LOW_DAMP_FACTOR 0.999
#define ATTACK_FACTOR 0.2

#define AMP_UPDATE_INTR_COUNT 50

static float freqs[48];
static audio_ctx_t ctx;
static int sin_lut[LUT_SIZE];

static const float  haramonic_piano[NUM_HARM] = {1, 0.4, 0.2, 0.1, 0.6, 0.15};
static const float  haramonic_flute[NUM_HARM] = {1, 0, 0, 0, 0, 0};
static const float  haramonic_misc[NUM_HARM] = {0.75, 0.2, 0.2, 0.2, 0.2, 0.2};
static const float  haramonic_nada[NUM_HARM] = {0.8, 0.6, 0.4, 0.2, 0.1, 0};

static const float* harmonic_amps[NUM_MODES] = {haramonic_piano, haramonic_flute, haramonic_misc, haramonic_nada};

static int intr_freq;

static int amp_update_counter;

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;
extern uint8_t sustain;
extern uint8_t chmod;
extern uint8_t mode;

void fill_freqs()
{
	double freq = LOWEST_FREQ;
	double m = pow(2, (double)1/12);
	for (int i = 0; i < 48; i++) {
		freqs[i] = (float) freq;
		freq *= m;
	}
	intr_freq = (int) freqs[47]*LUT_SIZE;
}

void fill_sin_lut()
{
	float* temp_haram = *harmonic_amps + mode;
	float harm_amp_sum = 0;
	for (int i = 0; i < NUM_HARM; i++) {
		harm_amp_sum += temp_haram[i];
	}

	// creates a sine wave look up table centered at VREF/2
	for(uint16_t i = 0; i < LUT_SIZE; ++i) {
		sin_lut[i] = 0;
		for (int j = 0; j < NUM_HARM; j++) {
//			sin_lut[i] += (int) (sin((double) 2 * PI * i * (j+1) / LUT_SIZE) * 2047 * harmonic_amps[j]);
			sin_lut[i] += (int) (sin((double) 2 * PI * i * (3*j/4 +1) / LUT_SIZE) * 2047 * (temp_haram[j]));
		}
		sin_lut[i] /= harm_amp_sum;
	}
}

const char *keys[12] = {"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "};

// disp_print(char *s, uint16_t x, uint16_t y, uint8_t size, uint16_t fg, uint16_t bg);

static void print_note(int note_idx)
{

	int mod = note_idx % 12;
	const uint16_t size = 10;
	const uint16_t x = (DISP_WIDTH - (2*CHAR_WIDTH + CHAR_PADDING) * size)/2;
	const uint16_t y = (DISP_HEIGHT - CHAR_HEIGHT * size)/2;
	disp_print(keys[mod], x, y, size, 0xa839, 0x0000);

}

const char *modes[NUM_MODES] = {"Piano", "Flute", "Guitar", "Electric"};

void print_mode()
{
	const uint16_t size = NUM_MODES;
	const uint16_t x = (DISP_WIDTH -150);
	const uint16_t y = (DISP_HEIGHT - 100);
	disp_print(modes[mode], x, y, size, 0xa839, 0x0000);
}

//const char* hail_to_victors[20] = {"A", "B", "C", "D", "E", "A", "B", "C", "D", "E", "A", "B", "C", "D", "E", "A", "B", "C", "D", "E"};
//
//void tutorial_mode(int note_idx){
//
//	  disp_fill_rect(0, 0, DISP_WIDTH, DISP_HEIGHT, BLACK);
//
//	  const uint16_t x = (DISP_WIDTH -150);
//	  const uint16_t y = (DISP_HEIGHT - 100);
//	  disp_print("Tutorial", x, y, size, 0xa839, 0x0000);
//
//	  int mistakes = 0;
//
//	  for(int i = 0; i < 20; i++) {
//		  char note_to_be_played = hail_to_the_victors[i];
//	  }
//}

// TODO: scale amplitude depending on frequency
void add_note(int note_idx, float note_amp)
{
	if (ctx.num_notes >= MAX_NOTES) {
		// ERROR or remove lowest amp note??
		return;
	}
//	float scaled_amp = note_amp + 0.2*((47-note_idx)/48);
	float scaled_amp = note_amp;
	uint8_t note_exists = 0;
	uint8_t existing_note_idx = MAX_NOTES + 1;

	// Check for existing notes and update amp back to one if present
	for (int i = 0; i < ctx.num_notes; i++) {
		if (ctx.notes[i] == note_idx) {
			note_exists = 1;
			existing_note_idx = i;
		}
	}
	if (!note_exists) {
		ctx.notes[ctx.num_notes] = note_idx;
		ctx.amps[ctx.num_notes] = scaled_amp;
		ctx.cycles[ctx.num_notes] = 0;
		ctx.cycles_per_wave[ctx.num_notes] = intr_freq / (int) freqs[note_idx];
		ctx.damp_factor &= ~(1 << ctx.num_notes);
		ctx.num_notes++;
	} else {
		ctx.amps[existing_note_idx] = scaled_amp;
		ctx.damp_factor &= ~(1 << existing_note_idx);
	}
	print_note(note_idx);
}


void set_damp_factor(int note, int high)
{
	for (int i = 0; i < ctx.num_notes; i++) {
		if (note == ctx.notes[i]) {
			if (high) {
				ctx.damp_factor |= (1 << i);
			}
			else {
				// Low
				ctx.damp_factor &= ~(1 << i);
			}
		}
	}
}

void update_amps()
{
	for (int i = 0; i < ctx.num_notes; i++) {
		if (sustain) {
			ctx.amps[i] *= LOW_DAMP_FACTOR;
		} else if (ctx.damp_factor & (1 << i)) { // decay fast
			ctx.amps[i] *= HIGH_DAMP_FACTOR;
		} else {
			ctx.amps[i] *= LOW_DAMP_FACTOR; //decay slowly
		}
	}
	for (int i = ctx.num_notes - 1; i >= 0; i--) {
		if (ctx.amps[i] <= DEAD_THRESHOLD) {
			// Remove note
			ctx.notes[i] = ctx.notes[ctx.num_notes - 1];
			ctx.amps[i] = ctx.amps[ctx.num_notes - 1];
			ctx.cycles[i] = ctx.cycles[ctx.num_notes - 1];
//			ctx.amps[ctx.num_notes - 1] = 0;
			ctx.cycles_per_wave[i] = ctx.cycles_per_wave[ctx.num_notes - 1];
			ctx.damp_factor &= ~(1 << i); // removing current note's damp factor
			ctx.damp_factor |= (1 << (ctx.num_notes - 1));
			ctx.num_notes--;
		}
	}

}

void init_audio_ctx()
{
	fill_freqs();
	fill_sin_lut();
	memset(&ctx, 0, sizeof(ctx));
	amp_update_counter = 0;
}

void audio_tim_isr()
{
	uint32_t index;
	float dac_out = 0;
	if (ctx.num_notes == 0) {
		return;
	}
	for (int i = 0; i < ctx.num_notes; i++) {
		ctx.cycles[i]++;
		if (ctx.cycles[i] >= ctx.cycles_per_wave[i]) {
			ctx.cycles[i] = 0;
		}
		index = ctx.cycles[i] * LUT_SIZE / ctx.cycles_per_wave[i];
		dac_out += ctx.amps[i] * sin_lut[index] + 2048;
	}
	dac_out /= ctx.num_notes;
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint32_t) dac_out);
	amp_update_counter++;
	if (amp_update_counter >= AMP_UPDATE_INTR_COUNT) {
		amp_update_counter = 0;
		update_amps();
	}
}

void init_timer(TIM_HandleTypeDef *htim)
{
	HAL_TIM_Base_Stop(&htim4);
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = PRESCALER;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = (uint32_t) (BASE_CLK/ (PRESCALER+1)/((float)LUT_SIZE)/freqs[47]);
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	HAL_TIM_Base_Init(&htim4);
	HAL_TIM_Base_Start_IT(&htim4);
}
