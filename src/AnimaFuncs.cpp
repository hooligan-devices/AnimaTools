#include "AnimaFuncs.h"

namespace AnimaFuncs
{
  CRGB blend_colors(const CRGB &col_A, const CRGB &col_B, float A_k)
  {
    CRGB mix;
    A_k = constrain(A_k, 0.0, 1.0);
    mix.r = roundf(A_k * col_A.r + (1.0 - A_k) * col_B.r);
    mix.g = roundf(A_k * col_A.g + (1.0 - A_k) * col_B.g);
    mix.b = roundf(A_k * col_A.b + (1.0 - A_k) * col_B.b);

    return mix;
  }

  CRGB rgb_from_hue(uint8_t hue_shift)
  {
    uint8_t rgb[3];
    const uint16_t hue_expanded = hue_shift * 6;

    uint8_t hue_i = hue_expanded / 256;          // current phase of total 6
    uint8_t tune_component = hue_expanded % 256; // leftover for current phase

    // component index of r/g/b which should be set to 0x0
    // wheel starts from zero_i == 2 (blue) and goes in circle [0 - 2]
    uint8_t zero_i = (hue_i / 2 + 2) % 3;
    uint8_t full_i_from_zero = hue_i % 2 + 1;         // full 0xFF component index, shifted from to zero_i
    uint8_t full_i = (zero_i + full_i_from_zero) % 3; // full 0xFF component index, in circle [0 - 2]

    rgb[zero_i] = 0;
    rgb[full_i] = 255;

    uint8_t tuned_i = 0; // component shoud be tuned
    if (full_i_from_zero == 1)
    {
      tuned_i = (zero_i + 2) % 3;
      rgb[tuned_i] = tune_component;
    }
    else
    {
      tuned_i = (zero_i + 1) % 3;
      rgb[tuned_i] = 255 - tune_component;
    }

    CRGB crgb;

    crgb.r = rgb[0];
    crgb.g = rgb[1];
    crgb.b = rgb[2];

    return crgb;
  }

  uint8_t get_hue(const CRGB &c)
  {
    uint8_t r = c.r;
    uint8_t g = c.g;
    uint8_t b = c.b;

    uint8_t maxc = max(r, max(g, b));
    uint8_t minc = min(r, min(g, b));

    // No hue (gray / black / white)
    if (maxc == minc)
      return 0;

    uint16_t range = maxc - minc; // 1..255
    int16_t hue;                  // 0..255 scaled hue

    if (maxc == r)
    {
      // Sector: Yellow↔Magenta, centered at 0
      hue = 0 + 43 * (int16_t)(g - b) / range;
    }
    else if (maxc == g)
    {
      // Sector: Cyan↔Yellow, centered at 85
      hue = 85 + 43 * (int16_t)(b - r) / range;
    }
    else
    { // maxc == b
      // Sector: Magenta↔Cyan, centered at 170
      hue = 170 + 43 * (int16_t)(r - g) / range;
    }

    if (hue < 0)
      hue += 256;
    if (hue > 255)
      hue -= 256;

    return (uint8_t)hue;
  }

  CRGB normalize_to_rgb(float r, float g, float b)
  {
    float scale = 1.0;

    if (r > g && r > b)
      scale = r / 255.0;
    else if (g > r && g > b)
      scale = g / 255.0;
    else if (b > r && b > g)
      scale = b / 255.0;

    r /= scale;
    g /= scale;
    b /= scale;

    CRGB rgb;
    rgb.r = static_cast<uint8_t>(r);
    rgb.g = static_cast<uint8_t>(g);
    rgb.b = static_cast<uint8_t>(b);

    return rgb;
  }

  inline float gamma_20_fast(float x)
  {
    return sqrtf(x);
  }

  CRGB rgb_from_xyz(float x, float y, float z)
  {
    float r_sum = 0.0;
    float g_sum = 0.0;
    float b_sum = 0.0;

    if (x >= 0)
      r_sum += x;
    else
    {
      g_sum -= x;
      b_sum -= x;
    }

    if (y >= 0)
      g_sum += y;
    else
    {
      r_sum -= y;
      b_sum -= y;
    }

    if (z >= 0)
      b_sum += z;
    else
    {
      r_sum -= z;
      g_sum -= z;
    }

    CRGB rgb = normalize_to_rgb(r_sum, g_sum, b_sum);

    return rgb;
  }

  float to_s_curve(float x, float exp)
  {
    // Keeps 0 and 1 fixed, bends around 0.5
    if (x < 0.5f)
      return 0.5f * powf(x * 2.0f, exp);
    else
      return 1.0f - 0.5f * powf((1.0f - x) * 2.0f, exp);
  }

}
