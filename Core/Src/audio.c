#include <stdint.h>
#include <string.h>
#include <math.h>
#include "stm32l4xx_hal.h"
#include "audio.h"

#define LUT_SIZE 512
#define INIT_AMP 0.7
#define DEAD_THRESHOLD 0.0001
#define NUM_HARM 4
#define PRESCALER 1
#define BASE_CLK 120000000
#define LOWEST_FREQ 110

#define PI 3.14159265
#define HIGH_DAMP_FACTOR 0.9
#define LOW_DAMP_FACTOR 1

#define AMP_UPDATE_INTR_COUNT 100


static float freqs[48];
static audio_ctx_t ctx;
static int sin_lut[LUT_SIZE];
static const float harmonic_amps[NUM_HARM] = {1, 0, 0, 0};
static int intr_freq;

static int amp_update_counter;

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;

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
	float harm_amp_sum = 0;
	for (int i = 0; i < NUM_HARM; i++) {
		harm_amp_sum += harmonic_amps[i];
	}

	// creates a sine wave look up table centered at VREF/2
	for(uint16_t i = 0; i < LUT_SIZE; ++i) {
		//sin_lut[i] = (int) (sin((double) 2 * PI * i / LUT_SIZE) * 2047);
		sin_lut[i] = 0;
		for (int j = 0; j < NUM_HARM; j++) {
			sin_lut[i] += (int) (sin((double) 2 * PI * i * (j+1) / LUT_SIZE) * 2047 * harmonic_amps[j]);
		}
		sin_lut[i] /= harm_amp_sum;
	}
}

void add_note(int note_idx)
{
	if (ctx.num_notes >= MAX_NOTES) {
		// ERROR or remove lowest amp note??
		return;
	}
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
		ctx.amps[ctx.num_notes] = INIT_AMP;
		ctx.cycles[ctx.num_notes] = 0;
		ctx.cycles_per_wave[ctx.num_notes] = intr_freq / (int) freqs[note_idx];
		ctx.damp_factor &= ~(1 << ctx.num_notes);
		ctx.num_notes++;
	} else {
		ctx.amps[existing_note_idx] = INIT_AMP;
	}
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
		if (ctx.damp_factor & (1 << i)) {
			ctx.amps[i] *= HIGH_DAMP_FACTOR;
		} else {
			ctx.amps[i] *= LOW_DAMP_FACTOR;
		}
	}
	for (int i = ctx.num_notes - 1; i >= 0; i--) {
		if (ctx.amps[i] <= DEAD_THRESHOLD) {
			// Remove note
			ctx.amps[i] = ctx.amps[ctx.num_notes - 1];
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
	if (index == 511) {
		int x = 7;
	}
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



















