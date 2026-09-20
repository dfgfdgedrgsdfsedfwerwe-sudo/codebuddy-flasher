#pragma once
#include "Arduino.h"
#include "Audio.h"
#include "SD_Card.h"
#include "initBoard.h"
// Digital I/O used
#define I2S_DOUT      45
#define I2S_BCLK      0  
#define I2S_LRC       38      // I2S_WS

// #define I2S_BLCK 0
// #define I2S_LRCK 38
// #define I2S_DSIN 39
// #define I2S_DOUT 45
// #define I2S_MCLK 3


#define EXAMPLE_Audio_TICK_PERIOD_MS  20
#define Volume_MAX  21


extern Audio audio;
extern uint8_t Volume;

void Play_Music_test();
void Audio_Loop();

void Audio_Init();
void Volume_adjustment(uint8_t Volume);
void Play_Music(const char* directory, const char* fileName);
void Music_pause(); 
void Music_resume();    
uint32_t Music_Duration();  
uint32_t Music_Elapsed();   
uint16_t Music_Energy();    
