#include "AnimaTools.h"

template <int N_LED_UNITS>
void IAnimaLeds<N_LED_UNITS>::connect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms)
{
  flow_params.update([&](flow_params_t &f)
                     {
    if (transition_ms > 0)
    {
      if (f.main_flow != nullptr)
        f.last_flow = f.main_flow;
  
      f.transition_start_ms = millis();
      f.transition_ms = transition_ms;
      f.is_transitioning = true;
    }
  
    f.main_flow = &anima_flow; });
}

template <int N_LED_UNITS>
void IAnimaLeds<N_LED_UNITS>::disconnect(millis_t transition_ms)
{
  flow_params.update([&](flow_params_t &f)
                     {
    f.last_flow = f.main_flow;
    f.transition_start_ms = millis();
    f.transition_ms = transition_ms;
    f.is_transitioning = true;
    f.main_flow = nullptr; });
}

template <int N_LED_UNITS>
void IAnimaLeds<N_LED_UNITS>::disconnect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms)
{
  bool is_connected = flow_params.read([&](const flow_params_t &f)
                                       { return f.main_flow == &anima_flow; });

  if (is_connected)
    disconnect(transition_ms);
}

template <int N_LED_UNITS>
bool IAnimaLeds<N_LED_UNITS>::is_connected(IAnimaFlow<N_LED_UNITS> &anima_flow)
{
  return flow_params.read([&](const flow_params_t &f)
                          { return f.main_flow == &anima_flow || f.last_flow == &anima_flow; });
}

template <int N_LED_UNITS>
float IAnimaLeds<N_LED_UNITS>::get_bright_headroom() const
{
  return BRIGHT_HEADROOM;
}

template <int N_LED_UNITS>
float IAnimaLeds<N_LED_UNITS>::get_bright_full_range() const
{
  return BRIGHT_FULL_RANGE;
}

ANIMA_LEDS_T_IMPL
ANIMA_LEDS_C::AnimaLeds()
{
  brightness.setup(
      this->BRIGHT_MIN_U8,
      this->BRIGHT_NOMINAL_MAX_U8,
      this->BRIGHT_ABSOLUTE_MAX_U8);

  set_brightness(1.0f);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds_FastLED, N_LED_UNITS * N_LEDS_IN_UNIT);
  //  LEDS.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(0xFFB0F0);  // Use this for WS2812
  // FastLED.setBrightness(brightness.get_ready_u8());
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::update_and_show()
{
  millis_t elapsed = 0;

  auto f = this->flow_params.read([&](const flow_params_t &fp)
                                  {
    if (fp.is_transitioning)
      elapsed = millis() - fp.transition_start_ms;
    return fp; });

  if (f.main_flow == nullptr && f.last_flow == nullptr)
    return;

  leds_t black_leds;
  rgb_array_t *rgb_flow = &black_leds.rgb;
  rgb_array_t *rgb_transition = &black_leds.rgb;
  float br_flow = 1.0f;
  float br_transition = 1.0f;
  bool transition_stopped = false;

  if (f.main_flow != nullptr)
  {
    f.main_flow->update();
    rgb_flow = &(f.main_flow->out_buf_leds.rgb);
    br_flow = f.main_flow->out_buf_leds.brightness;
  }

  if (f.last_flow != nullptr)
  {
    f.last_flow->update();
    rgb_transition = &(f.last_flow->out_buf_leds.rgb);
    br_transition = f.last_flow->out_buf_leds.brightness;
  }

  // === blend main with transition flow
  if (f.is_transitioning)
  {
    float scale = 1.0f;
    if (f.transition_ms == 0)
    {
      scale = 1.0f;
    }
    else
    {
      scale = (float)elapsed / (float)f.transition_ms;
      scale = constrain(scale, 0.0f, 1.0f);
    }

    for (size_t i = 0; i < N_LED_UNITS; i++)
      (*rgb_flow)[i] = AnimaFuncs::blend_colors((*rgb_flow)[i], (*rgb_transition)[i], scale);

    br_flow = br_flow * scale + br_transition * (1.0f - scale);

    this->flow_params.update([&](flow_params_t &fp)
                             {
    if (elapsed > fp.transition_ms)
    {
      fp.is_transitioning = false;
      fp.last_flow = nullptr; // disconnect
      transition_stopped = true;
    } });
  }

  // === fill output leds: leds_FastLED
  if (N_LEDS_IN_UNIT == 1)
    for (size_t i = 0; i < N_LED_UNITS; i++)
      leds_FastLED[i] = (*rgb_flow)[i];
  else
  {
    for (size_t i = 0; i < N_LED_UNITS; i++)
      for (size_t j = 0; j < N_LEDS_IN_UNIT; j++)
      {
        size_t index = i * N_LEDS_IN_UNIT + j;
        leds_FastLED[index] = (*rgb_flow)[i];
      }
  }

  brightness_control.update([&](br_control_t &br)
                            { br.update(brightness); });

  FastLED.setBrightness(brightness.get_ready_u8(br_flow));
  FastLED.show();

  if (is_handle_off_state && transition_stopped)
  {
    FastLED.clear();
    pinMode(LED_PIN, OUTPUT);
    is_handle_off_state = false;
  }
}

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::AnimaFlow(IAnimaLeds<N_LED_UNITS> &anima_leds)
    : anima_leds(anima_leds)
{
  set_brightness_now(1.0f);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::connect(millis_t transition_ms)
{
  anima_leds.connect(*this, transition_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::disconnect(millis_t transition_ms)
{
  anima_leds.disconnect(*this, transition_ms);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::is_connected()
{
  return anima_leds.is_connected(*this);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_mask(uint8_t layer_i, const mask_array_t &mask)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.set(mask, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_mask_now(uint8_t layer_i, const mask_array_t &mask)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.set(mask, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_colors(uint8_t layer_i, const rgb_array_t &rgb)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_colors(rgb, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_colors_now(uint8_t layer_i, const rgb_array_t &rgb)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                               { c.set_colors(rgb, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_solid(uint8_t layer_i, const CRGB &color)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_solid(color, false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_solid(uint8_t layer_i, const CRGB &color, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_solid(color, start_i, end_i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_solid_now(uint8_t layer_i, const CRGB &color)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                                { c.set_solid(color, true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_solid_now(uint8_t layer_i, const CRGB &color, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                                { c.set_solid(color, start_i, end_i, true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_hue_now(uint8_t layer_i, uint8_t num_sectors, uint8_t units_per_color, uint8_t hue_shift)
{
  if (layer_i >= N_LAYERS)
    return;

  uint8_t sector_lengh = 256 / num_sectors;
  rgb_array_t rgb;

  for (size_t i = 0; i < N_LED_UNITS; i++)
  {
    uint8_t color_i = i / units_per_color;
    uint8_t sector_i = color_i % num_sectors;
    uint8_t hue = ((uint16_t)hue_shift + sector_i * sector_lengh) % 256;
    rgb[i] = AnimaFuncs::rgb_from_hue(hue);
  }

  set_colors_now(layer_i, rgb);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::is_opacity_idle(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                      { return a.is_idle(); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_opacity(uint8_t layer_i, float opac)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.set_k(opac, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_opacity_now(uint8_t layer_i, float opac)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.set_k(opac, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::fade_opacity(uint8_t layer_i, float opac_dest, millis_t duration_ms)
{
  if (layer_i >= N_LAYERS)
    return false;

  if (duration_ms > 0)
    return alpha_control[layer_i].update([&](k_control_t &a)
                                         { return a.start_fade(opac_dest, duration_ms, false); });
  else
    return set_opacity(layer_i, opac_dest);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::fade_opacity_now(uint8_t layer_i, float opac_dest, millis_t duration_ms)
{
  if (layer_i >= N_LAYERS)
    return;

  if (duration_ms > 0)
    alpha_control[layer_i].update([&](k_control_t &a)
                                  { a.start_fade(opac_dest, duration_ms, true); });
  else
    set_opacity_now(layer_i, opac_dest);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::show_layer(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    return set_opacity(layer_i, 1.0);
  else
    return fade_opacity(layer_i, 1.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::show_layer_now(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    set_opacity_now(layer_i, 1.0);
  else
    fade_opacity_now(layer_i, 1.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::hide_layer(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    return set_opacity(layer_i, 0.0);
  else
    return fade_opacity(layer_i, 0.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::hide_layer_now(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    set_opacity_now(layer_i, 0.0);
  else
    fade_opacity_now(layer_i, 0.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::apply_periodic_opacity(uint8_t layer_i, const AnimaPeriodic &periodic)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.apply_periodic(periodic); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_periodic_opacity(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.start_periodic(false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_periodic_opacity(uint8_t layer_i, const AnimaPeriodic &periodic)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.start_periodic(periodic, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_periodic_opacity_now(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.start_periodic(true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_periodic_opacity_now(uint8_t layer_i, const AnimaPeriodic &periodic)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.start_periodic(periodic, true); });
}

// ANIMA_TOOLS_T_IMPL
// void ANIMA_TOOLS_C::run_and_update_shake(uint8_t layer_i, bool is_shaking, float shaking_curve)
// {
//   static uint8_t phase = 0;
//   static uint32_t hue_shift = 0;

//   layer_t &l = layer[layer_i];

//   if (l.owned_by != MANAGER_TYPE::LR_SHAKE)
//   {
//     if (is_shaking)
//     {
//       l.owned_by = MANAGER_TYPE::LR_SHAKE;
//       phase = 0;
//     }
//     else
//       return;
//   }

//   if (phase == 0)
//   {
//     fade_opacity_(layer_i, 300, 1.0);
//     hue_shift = 0;
//     phase++;
//   }
//   else if (phase == 1)
//   {
//     if (!is_fade_opacity_running(layer_i))
//       phase++;
//   }
//   else if (phase == 2)
//   {
//     if (!is_shaking)
//       phase++;
//   }
//   else if (phase == 3)
//   {
//     fade_opacity_(layer_i, 700, 0.0);
//     phase++;
//   }
//   else if (phase == 4)
//   {
//     if (!is_fade_opacity_running(layer_i))
//       l.owned_by = MANAGER_TYPE::NONE;
//     return;
//   }

//   uint32_t hue_shift_per_s = abs(shaking_curve) * 2900;
//   hue_shift += hue_shift_per_s / UPDATE_FREQUENCY_HZ; // { 73 = (256 / 7) * 2 }
//   hue_shift %= 256;
//   set_hue_now(layer_i, 6, hue_shift);

//   float sh = constrain(shaking_curve, -0.3, 0.3);
//   set_brightness_(0.7 + sh);
// }

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_brightness(float br)
{
  return brightness_control.update([&](k_control_t &b)
                                   { return b.set_k(br, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_brightness_now(float br)
{
  brightness_control.update([&](k_control_t &b)
                            { b.set_k(br, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::fade_brightness(float bright_dest, millis_t duration_ms)
{
  if (duration_ms > 0)
    return brightness_control.update([&](k_control_t &b)
                                     { return b.start_fade(bright_dest, duration_ms, false); });
  else
    return set_brightness(bright_dest);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::fade_brightness_now(float bright_dest, millis_t duration_ms)
{
  if (duration_ms > 0)
    brightness_control.update([&](k_control_t &b)
                              { b.start_fade(bright_dest, duration_ms, true); });
  else
    set_brightness_now(bright_dest);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::apply_periodic_brightness(const AnimaPeriodic &periodic, bool apply_dc_offset)
{
  if (apply_dc_offset)
  {
    AnimaPeriodic p = periodic;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    brightness_control.update([&](k_control_t &b)
                              { b.apply_periodic(p); });
    return;
  }

  brightness_control.update([&](k_control_t &b)
                            { b.apply_periodic(periodic); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_periodic_brightness()
{
  return brightness_control.update([&](k_control_t &b)
                                   { return b.start_periodic(false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_periodic_brightness(const AnimaPeriodic &periodic, bool apply_dc_offset)

{
  if (apply_dc_offset)
  {
    AnimaPeriodic p = periodic;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    return brightness_control.update([&](k_control_t &b)
                                     { return b.start_periodic(p, false); });
  }

  return brightness_control.update([&](k_control_t &b)
                                   { return b.start_periodic(periodic, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_periodic_brightness_now()
{
  brightness_control.update([&](k_control_t &b)
                            { b.start_periodic(true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_periodic_brightness_now(const AnimaPeriodic &periodic, bool apply_dc_offset)
{
  if (apply_dc_offset)
  {
    AnimaPeriodic p = periodic;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    brightness_control.update([&](k_control_t &b)
                              { b.start_periodic(p, true); });
    return;
  }

  brightness_control.update([&](k_control_t &b)
                            { b.start_periodic(periodic, true); });
}

ANIMA_FLOW_T_IMPL
int ANIMA_FLOW_C::get_n_led_units() const
{
  return N_LED_UNITS;
}

ANIMA_FLOW_T_IMPL
uint8_t ANIMA_FLOW_C::get_n_layers() const
{
  return N_LAYERS;
}

ANIMA_FLOW_T_IMPL
float ANIMA_FLOW_C::get_bright_headroom() const
{
  return anima_leds.get_bright_headroom();
}

ANIMA_FLOW_T_IMPL
float ANIMA_FLOW_C::get_bright_full_range() const
{
  return anima_leds.get_bright_full_range();
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::update()
{
  for (size_t i = 0; i < N_LAYERS; i++)
  {
    mask_control[i].update([&, i](mask_control_t &m)
                           { m.update(mask[i]); });

    color_control[i].update([&, i](color_control_t &c)
                            { c.update(color[i]); });

    alpha_control[i].update([&, i](k_control_t &a)
                            { a.update(alpha[i]); });
  }

  brightness_control.update([&](k_control_t &b)
                            { b.update(brightness); });

  make_output_leds();
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::make_output_leds()
{
  if (N_LAYERS <= 0)
    return;

  // Initialize with top layer
  int top_layer = N_LAYERS - 1;
  const mask_array_t &top_mask = mask[top_layer].get();
  bool top_mask_active = mask[top_layer].is_active();
  const rgb_array_t &top_rgb = color[top_layer].get_rgb();
  float top_alpha = alpha[top_layer].get();

  if (top_mask_active)
    for (size_t i = 0; i < N_LED_UNITS; i++)
      this->out_buf_leds.rgb[i] = top_mask[i] ? top_rgb[i] : CRGB::Black;
  else
    this->out_buf_leds.rgb = top_rgb;

  float layer_alpha = 0.0f;

  // Merge remaining layers from top-1 to bottom
  for (int layer_i = top_layer - 1; layer_i >= 0; layer_i--)
  {
    layer_alpha = alpha[layer_i].get();

    if (layer_alpha <= 0.0f)
      continue;

    const mask_array_t &layer_mask = mask[layer_i].get();
    bool layer_mask_active = mask[layer_i].is_active();
    const rgb_array_t &layer_rgb = color[layer_i].get_rgb();

    if (!layer_mask_active && !top_mask_active)
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], layer_rgb[i], top_alpha);
    }
    else if (!layer_mask_active && top_mask_active)
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        if (top_mask[i])
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], layer_rgb[i], top_alpha);
        else
          this->out_buf_leds.rgb[i] = layer_rgb[i];
    }
    else if (layer_mask_active && !top_mask_active)
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        if (layer_mask[i])
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], layer_rgb[i], top_alpha);
    }
    else // masks for both blending layers active
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        if (top_mask[i] && layer_mask[i])
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], layer_rgb[i], top_alpha);
        else if (layer_mask[i])
          this->out_buf_leds.rgb[i] = layer_rgb[i];
    }

    if (layer_alpha >= 1.0f)
      break;

    top_alpha += layer_alpha * (1.0f - top_alpha);
  }

  // processing bottom layer if not fully opaque
  if (layer_alpha < 1.0f)
  {
    const mask_array_t &layer_mask = mask[0].get();
    bool layer_mask_active = mask[0].is_active();

    if (layer_mask_active)
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        if (layer_mask[i])
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], CRGB::Black, layer_alpha);
        else
          this->out_buf_leds.rgb[i] = CRGB::Black;
    }
    else
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], CRGB::Black, layer_alpha);
    }
  }

  // Copy brightness
  this->out_buf_leds.brightness = brightness.get();
}
