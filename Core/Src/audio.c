#include <stdint.h>
#include <string.h>
#include <math.h>
#include "stm32l4xx_hal.h"
#include "audio.h"

#define WAVE_SAMPLES 256
#define INIT_AMP 1
#define DEAD_THRESHOLD 0.001
#define NUM_HARM 4
#define PRESCALER 1
#define BASE_CLK 120000000

#define PI 3.14159265
#define HIGH_DAMP_FACTOR 0.8
#define LOW_DAMP_FACTOR 1


static float freqs[48];
static audio_ctx_t ctx;
static uint16_t sin_lut[WAVE_SAMPLES];
static const float harmonic_amps[NUM_HARM] = {1, 0.2, 0.1};
static int intr_freq;

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;

void fill_freqs()
{
	double freq = 55;
	double m = pow(2, (double)1/12);
	for (int i = 0; i < 48; i++) {
		freqs[i] = (float) freq;
		freq *= m;
	}
	intr_freq = (int) freqs[47]*WAVE_SAMPLES;
}

void fill_sin_lut()
{
	// creates a sine wave look up table centered at VREF/2
	for(uint16_t i = 0; i < WAVE_SAMPLES; ++i) {
		sin_lut[i] = 0;
		for (int j = 0; j < NUM_HARM; j++) {
			sin_lut[i] = (uint16_t) (sin((double) 2 * PI * i * (j+1) / WAVE_SAMPLES) * 2047 + 2048);
		}
	}
}

void add_note(int note_idx)
{
	if (ctx.num_notes >= MAX_NOTES) {
		// ERROR or remove lowest amp note??
		return;
	}
	ctx.notes[ctx.num_notes] = note_idx;
	ctx.amps[ctx.num_notes] = INIT_AMP;
	ctx.cycles[ctx.num_notes] = 0;
	ctx.cycles_per_wave[ctx.num_notes] = intr_freq / (int) freqs[note_idx];

	ctx.num_notes++;
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
}

void audio_tim_isr()
{
	uint32_t index;
	float dac_out = 0;
	for (int i = 0; i < ctx.num_notes; i++) {
		ctx.cycles[i] = (ctx.cycles[i] + 1) % ctx.cycles_per_wave[i];
		index = ctx.cycles[i] * WAVE_SAMPLES / ctx.cycles_per_wave[i];
		dac_out += ctx.amps[i] * sin_lut[index];
	}
	dac_out /= ctx.num_notes;
	update_amps();
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint32_t) dac_out);
}

void init_timer(TIM_HandleTypeDef *htim)
{
	HAL_TIM_Base_Stop(&htim4);
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = PRESCALER;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = (uint32_t) (BASE_CLK/ (PRESCALER+1)/((float)WAVE_SAMPLES)/freqs[47]);
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	HAL_TIM_Base_Init(&htim4);
	HAL_TIM_Base_Start_IT(&htim4);
}


















