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
#define LOWEST_FREQ 65.41

#define PI 3.14159265
#define HIGH_DAMP_FACTOR 0.95
#define LOW_DAMP_FACTOR 0.9992
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

/* TUTORIAL MODE VARS*/

static uint8_t tutorial_index;
static const char* correct_tut_notes[NUM_TUT_NOTES] = {"E ", "C ", "D ", "E ", "C ", "D ", "E ", "F ",
												"D ", "E ", "F ", "D ", "E ", "F ", "G ", "A ", "F ",
												"E ", "F ", "C ", "D ", "E ", "G ", "E ", "D ", "C "};
//static uint8_t correct_tut_notes[NUM_TUT_NOTES] = {3, 1, 2, 3, 1, 2, 3, 3,
//												1, 2, 3, 1, 2, 3, 2, 3,
//												1, 2, 3, 1, 1, 2, 3, 3, 2, 1};

extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim4;
extern uint8_t sustain;
extern uint8_t chmod;
extern uint8_t mode;
extern tutorial_mode;
extern best_index;
extern pressure_wait;

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

const char *modes[NUM_MODES] = {"Piano", "Alt Sax", "Bright", "Electric"};

void print_mode()
{
	uint8_t mode_to_clear = (mode == 0) ? 3 : mode - 1;

	const uint16_t size = NUM_MODES;
	const uint16_t x = (DISP_WIDTH -180);
	const uint16_t y = (DISP_HEIGHT - 60);
	disp_print(modes[mode_to_clear], x, y, size, 0x0000, 0x0000);
	disp_print(modes[mode], x, y, size, 0x0000, 0x0000);
	disp_print(modes[mode], x, y, size, 0xa839, 0x0000);
}

// TODO: scale amplitude depending on frequency
void add_note(int note_idx, float note_amp)
{
	if (ctx.num_notes >= MAX_NOTES) {
		// ERROR or remove lowest amp note??
		return;
	}

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
	// if in tutorial mode ? check for whether note is correct : nothinbg
	if (!tutorial_mode) {
		print_note(note_idx);
	} else {
		tutorial_check_note(note_idx);
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
	tutorial_index = 0;
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


/* TUTORIAL MODE CODE*/

void tutorial_check_note(uint8_t note_idx)
{
	// have current tutorial index
	// when note correctly played, increment tutorial index
	// else handle incorrect note actions
	uint8_t size = 10;
	uint8_t tut_c_x = (DISP_WIDTH - (2*CHAR_WIDTH + CHAR_PADDING) * size)/2;
	uint8_t tut_c_y = (DISP_HEIGHT - CHAR_HEIGHT * size)/4;
	disp_print(correct_tut_notes[tutorial_index], CORR_X, CORR_Y, size, 0xf01d, 0x0000);
	HAL_Delay(500);
	if (is_note_correct(note_idx)) {
		handle_note_correct(note_idx);
	} else {
		handle_note_incorrect(note_idx);
	}
}

uint8_t is_note_correct(uint8_t note_idx)
{
	uint8_t mod = note_idx%12;
	if (keys[mod] == correct_tut_notes[tutorial_index]) {
		return 1;
	} else {
		return 0;
	}
}

const char* fingers[5] = {"Little", "Ring", "Middle", "Pointer", "Thumb"};
static uint8_t fingers_cnt[5] = {0,0,0,0,0};

void handle_note_correct(uint8_t note_idx)
{
	// display note in green
	// increment tutorial note index
	int mod = note_idx % 12;
	uint8_t size = 10;
	uint8_t tut_c_x = (DISP_WIDTH - (2*CHAR_WIDTH + CHAR_PADDING) * size)/2;
	uint8_t tut_c_y = 3*(DISP_HEIGHT - CHAR_HEIGHT * size)/4;
	disp_print(keys[mod], tut_c_x, tut_c_y, size, 0x07e0, 0x0000);

	if (tutorial_index >= NUM_TUT_NOTES) {
		tutorial_mode = 0;
		tutorial_index = 0;
		disp_print(keys[mod], tut_c_x, tut_c_y, size, 0x0000, 0x0000);
		disp_print(correct_tut_notes[tutorial_index], CORR_X, CORR_Y, size, 0x0000, 0x0000);
		disp_print("Good Stuff Boss", 10, tut_c_y/2, 5, 0x0f6f, 0x0000);
		disp_print("Tutorial", TUT_X, TUT_Y, 4, 0x0000, 0x0000);
		HAL_Delay(3000);
		disp_print("Good Stuff Boss", 10, tut_c_y/2, 5, 0x0000, 0x0000);
		disp_print("Fingers Pressed", 10, tut_c_y/2 - 40, 5, 0x0f6f, 0x0000);
		for (int i = 0; i < 5; i++) {
			if (fingers_cnt[i] > 0) {
				disp_print(fingers[i], 10, tut_c_y/2, 5, 0x0f6f, 0x0000);
				HAL_Delay(1000);
				disp_print(fingers[i], 10, tut_c_y/2, 5, 0x0000, 0x0000);
			}
		}
		memset(fingers_cnt, 5, 0);
	}
	else {
		tutorial_index++;
		HAL_Delay(500);
		disp_print(keys[mod], tut_c_x, tut_c_y, size, 0x0000, 0x0000);
		disp_print(correct_tut_notes[tutorial_index], CORR_X, CORR_Y, size, 0xf01d, 0x0000);

		if(best_index != -1){
			fingers_cnt[best_index]++;
			pressure_wait = 1;
//			while(pressure_wait);
//			disp_print(fingers[best_index], DISP_HEIGHT -10, 2, 5, 0x0f6f, 0x0000);
			best_index = -1;
		}
	}
}

void handle_note_incorrect(uint8_t note_idx)
{
	// display note in red
	int mod = note_idx % 12;
	uint8_t size = 10;
	uint8_t tut_c_x = (DISP_WIDTH - (2*CHAR_WIDTH + CHAR_PADDING) * size)/2;
	uint8_t tut_c_y = 3*(DISP_HEIGHT - CHAR_HEIGHT * size)/4 ;
	disp_print(keys[mod], tut_c_x, tut_c_y, size, 0xd141, 0x0000);
	HAL_Delay(1000);
	disp_print(keys[mod], tut_c_x, tut_c_y, size, 0x0000, 0x0000);
}
void tut_init_display() {
	disp_fill_rect(0, 0, DISP_WIDTH, DISP_HEIGHT, BLACK);
	disp_print("Tutorial", TUT_X, TUT_Y, 4, 0xf81c, 0x0000);
	disp_print("Hail To The Victors", TUT_X, TUT_Y+40, 4, 0xffc0, 0x0000);
	HAL_Delay(2000);
	disp_print("Hail To The Victors", TUT_X, TUT_Y+40, 4, 0x0000, 0x0000);
	disp_print("Follow the notes on", TUT_X, TUT_Y+50, 4, 0xffc0, 0x0000);
	disp_print("the screen", TUT_X, TUT_Y+90, 4, 0xffc0, 0x0000);
	HAL_Delay(2000);
	disp_print("Follow the notes on", TUT_X, TUT_Y+50, 4, 0x0000, 0x0000);
	disp_print("the screen", TUT_X, TUT_Y+90, 4, 0x0000, 0x0000);
	disp_print(correct_tut_notes[tutorial_index], CORR_X, CORR_Y, 10, 0xf01d, 0x0000);
}


