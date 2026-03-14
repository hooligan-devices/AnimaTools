#pragma once
#include <Arduino.h>
#include <FastLED.h>

namespace AnimaFuncs
{
    CRGB blend_colors(const CRGB &col_A, const CRGB &col_B, float A_k);
    CRGB rgb_from_hue(uint8_t hue_shift);
    uint8_t get_hue(const CRGB &c);
    CRGB normalize_to_rgb(float r, float g, float b);
    CRGB rgb_from_xyz(float x, float y, float z);
    float to_s_curve(float x, float exp);
}

