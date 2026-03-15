// === Anima Flow Interface ===

// ---
// ++ I_ANIMA_FLOW_C::leds_t

I_ANIMA_FLOW_T_IMPL
I_ANIMA_FLOW_C::leds_t::leds_t()
{
  rgb.fill(CRGB::Black);
}

// ---
// ++ I_ANIMA_FLOW_C

I_ANIMA_LEDS_T_IMPL
void I_ANIMA_LEDS_C::connect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms)
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

// === Anima Leds Interface ===

// ---
// ++ I_ANIMA_LEDS_C

I_ANIMA_LEDS_T_IMPL
void I_ANIMA_LEDS_C::disconnect(millis_t transition_ms)
{
  flow_params.update([&](flow_params_t &f)
                     {
    f.last_flow = f.main_flow;
    f.transition_start_ms = millis();
    f.transition_ms = transition_ms;
    f.is_transitioning = true;
    f.main_flow = nullptr; });
}

I_ANIMA_LEDS_T_IMPL
void I_ANIMA_LEDS_C::disconnect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms)
{
  bool is_connected = flow_params.read([&](const flow_params_t &f)
                                       { return f.main_flow == &anima_flow; });

  if (is_connected)
    disconnect(transition_ms);
}

I_ANIMA_LEDS_T_IMPL
bool I_ANIMA_LEDS_C::is_connected(IAnimaFlow<N_LED_UNITS> &anima_flow)
{
  return flow_params.read([&](const flow_params_t &f)
                          { return f.main_flow == &anima_flow || f.last_flow == &anima_flow; });
}

I_ANIMA_LEDS_T_IMPL
float I_ANIMA_LEDS_C::get_bright_headroom() const
{
  return BRIGHT_HEADROOM;
}

I_ANIMA_LEDS_T_IMPL
float I_ANIMA_LEDS_C::get_bright_full_range() const
{
  return BRIGHT_FULL_RANGE;
}

// === Anima Leds Implementation ===
// === Private

// ---
// ++ ANIMA_LEDS_C::brightness_state_t

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::brightness_state_t::setup(uint8_t br_min_u8, uint8_t br_nom_max_u8, uint8_t br_abs_max_u8)
{
  this->br_abs_max_u8 = br_abs_max_u8;
  this->br_nom_max_u8 = br_nom_max_u8;
  min_device_br = (float)br_min_u8 / (float)br_nom_max_u8;
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::brightness_state_t::update(float device_brightness, float on_off_fade_k)
{
  this->device_brightness = device_brightness;
  device_br_scaled = device_brightness * (1.0f - min_device_br) + min_device_br;
  this->on_off_fade_k = on_off_fade_k;
}

ANIMA_LEDS_T_IMPL
float ANIMA_LEDS_C::brightness_state_t::get_device_brightness() const
{
  return device_brightness;
}

ANIMA_LEDS_T_IMPL
uint8_t ANIMA_LEDS_C::brightness_state_t::get_ready_u8(float br_flow) const
{
  int br = br_flow * device_br_scaled * on_off_fade_k * br_nom_max_u8;
  br = constrain(br, 0, br_abs_max_u8);
  return uint8_t(br);
}

// ---
// ++ ANIMA_LEDS_C::br_control_t

ANIMA_LEDS_T_IMPL
float ANIMA_LEDS_C::br_control_t::get() const
{
  return device_brightness;
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::br_control_t::set(float br, millis_t fade_ms)
{
  br = constrain(br, 0.0f, 1.0f);
  fader_global.start(device_brightness, br, fade_ms);
  is_animating_ = true;
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::br_control_t::on(millis_t fade_ms)
{
  fader_on_off.start(on_off_fade_k, 1.0f, fade_ms);
  is_animating_ = true;
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::br_control_t::off(millis_t fade_ms)
{
  fader_on_off.start(on_off_fade_k, 0.0f, fade_ms);
  is_animating_ = true;
}

ANIMA_LEDS_T_IMPL
bool ANIMA_LEDS_C::br_control_t::is_animating() const
{
  return is_animating_;
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::br_control_t::update(brightness_state_t &br_state)
{
  if (!is_animating_)
    return;

  if (fader_global.update())
    device_brightness = fader_global.get_k();
  if (fader_on_off.update())
    on_off_fade_k = fader_on_off.get_k();

  br_state.update(device_brightness, on_off_fade_k);

  if (!fader_global.is_running() && !fader_on_off.is_running())
    is_animating_ = false;
}

// === Anima Leds Implementation ===
// === Public

// ---
// ++ ANIMA_LEDS_C

ANIMA_LEDS_T_IMPL
ANIMA_LEDS_C::AnimaLeds()
{
  brightness.setup(
      this->BRIGHT_MIN_U8,
      this->BRIGHT_NOMINAL_MAX_U8,
      this->BRIGHT_ABSOLUTE_MAX_U8);

  set_brightness(1.0f);

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds_FastLED, N_LED_UNITS * N_LEDS_IN_UNIT);
  // LEDS.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(0xFFB0F0);  // Use this for WS2812
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

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::set_brightness(float br)
{
  brightness_control.update([&](br_control_t &b)
                            { b.set(br); });
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::set_brightness(float br, millis_t fade_ms)
{
  brightness_control.update([&](br_control_t &b)
                            { b.set(br, fade_ms); });
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::on()
{
  brightness_control.update([&](br_control_t &b)
                            { b.on(); });
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::on(millis_t fade_ms)
{
  brightness_control.update([&](br_control_t &b)
                            { b.on(fade_ms); });
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::off()
{
  is_handle_off_state = true;
  brightness_control.update([&](br_control_t &b)
                            { b.off(); });
}

ANIMA_LEDS_T_IMPL
void ANIMA_LEDS_C::off(millis_t fade_ms)
{
  is_handle_off_state = true;
  brightness_control.update([&](br_control_t &br)
                            { br.off(fade_ms); });
}

ANIMA_LEDS_T_IMPL
bool ANIMA_LEDS_C::is_animating() const
{
  bool is_bright_running = brightness_control.read([](const br_control_t &br)
                                                   { return br.is_animating(); });

  bool is_transitioning = this->flow_params.read([](const flow_params_t &fp)
                                                 { return fp.is_transitioning; });

  return is_bright_running || is_transitioning;
}

// === Anima Flow Implementation ===
// === Private

// ---
// ++ ANIMA_FLOW_C::mask_state_t

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::mask_state_t::mask_state_t()
{
  mask.fill(true);
}

ANIMA_FLOW_T_IMPL
const typename ANIMA_FLOW_C::mask_array_t &
ANIMA_FLOW_C::mask_state_t::get() const
{
  return mask;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::mask_state_t::set(const mask_array_t &mask)
{
  this->mask = mask;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_state_t::is_active() const
{
  return mask_is_active;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::mask_state_t::set_active(bool active)
{
  mask_is_active = active;
}

// ---
// ++ ANIMA_FLOW_C::mask_control_t

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::mask_control_t::mask_control_t()
{
  mask.fill(true);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::set(const mask_array_t &mask, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  this->mask = mask;
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::enable_all(bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  // mask not used on updates when mask_is_active == false
  // but needs to set anyway to store state to use selective setters afterwards (like single & range)
  mask.fill(true);
  this->mask_is_active = false;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::disable_all(bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  mask.fill(false);
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::enable_single(int i, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;
  if (i >= mask.size())
    return false;

  owner.request(MANAGER::SETTER);
  mask[i] = true;
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::disable_single(int i, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;
  if (i >= mask.size())
    return false;

  owner.request(MANAGER::SETTER);
  mask[i] = false;
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::enable_range(int start_i, int end_i, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  if (start_i < 0)
    start_i = 0;

  if (end_i > mask.size())
    end_i = mask.size();

  if (start_i >= end_i)
    return false;

  owner.request(MANAGER::SETTER);
  std::fill(mask.begin() + start_i, mask.begin() + end_i, true);
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::mask_control_t::disable_range(int start_i, int end_i, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  if (start_i < 0)
    start_i = 0;

  if (end_i > mask.size())
    end_i = mask.size();

  if (start_i >= end_i)
    return false;

  owner.request(MANAGER::SETTER);
  std::fill(mask.begin() + start_i, mask.begin() + end_i, false);
  this->mask_is_active = true;
  return true;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::mask_control_t::update(mask_state_t &m)
{
  owner.apply_request();

  if (owner.is_none())
    return;

  switch (owner.get())
  {
  case MANAGER::SETTER:
    owner.set_none();
    break;
  default:
    break;
  }

  m.set_active(mask_is_active);
  if (m.is_active())
    m.set(mask);
}

// ---
// ++ ANIMA_FLOW_C::color_state_t

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::color_state_t::color_state_t()
{
  rgb.fill(CRGB::Black);
}

ANIMA_FLOW_T_IMPL
const typename ANIMA_FLOW_C::rgb_array_t &
ANIMA_FLOW_C::color_state_t::get_rgb() const
{
  return rgb;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::color_state_t::set_rgb(const rgb_array_t &rgb)
{
  this->rgb = rgb;
}

// ---
// ++ ANIMA_FLOW_C::color_control_t

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::color_control_t::color_control_t()
{
  rgb.fill(CRGB::Black);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::color_control_t::set_colors(rgb_array_t rgb_arr, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  rgb = rgb_arr;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::color_control_t::set_solid(CRGB color, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  rgb.fill(color);
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::color_control_t::set_solid(CRGB color, int start_i, int end_i, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  if (start_i < 0)
    start_i = 0;

  if (end_i > rgb.size())
    end_i = rgb.size();

  if (start_i >= end_i)
    return false;

  owner.request(MANAGER::SETTER);
  std::fill(rgb.begin() + start_i, rgb.begin() + end_i, color);
  return true;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::color_control_t::update(color_state_t &l)
{
  owner.apply_request();

  if (owner.is_none())
    return;

  switch (owner.get())
  {
  case MANAGER::SETTER:
    owner.set_none();
    break;
  default:
    break;
  }

  l.set_rgb(rgb);
}

// ---
// ++ ANIMA_FLOW_C::k_state_t

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::k_state_t::set(float k)
{
  this->k = k;
}

ANIMA_FLOW_T_IMPL
float ANIMA_FLOW_C::k_state_t::get() const
{
  return k;
}

// ---
// ++ ANIMA_FLOW_C::k_control_t

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::get_k() const
{
  return current_k;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::set_k(float k, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::SETTER);
  current_k = k;
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::start_fade(float k_dest, millis_t duration_ms, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::FADER);
  fader.start(current_k, k_dest, duration_ms);
  return true;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::k_control_t::apply_envelope(const AnimaEnvelope &env)
{
  envelope_runner.apply(env);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::start_envelope(bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::ENVELOPE);
  envelope_runner.start(current_k);
  return true;
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::start_envelope(const AnimaEnvelope &env, bool terminate)
{
  if (owner.is_some() && !terminate)
    return false;

  owner.request(MANAGER::ENVELOPE);
  envelope_runner.start(env, current_k);
  return true;
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::k_control_t::stop_envelope()
{
  envelope_runner.stop();
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::k_control_t::terminate_envelope()
{
  envelope_runner.terminate();
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::k_control_t::is_free() const
{
  return owner.is_free();
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::k_control_t::update(k_state_t &k)
{
  owner.apply_request();

  if (owner.is_none())
    return;

  switch (owner.get())
  {
  case MANAGER::SETTER:
    owner.set_none();
    break;
  case MANAGER::FADER:
    if (fader.update())
      current_k = fader.get_k();

    if (!fader.is_running())
      owner.set_none();
    break;
  case MANAGER::ENVELOPE:
    if (envelope_runner.update(fader))
      current_k = envelope_runner.get_level();
    else
      owner.set_none();
    break;
  default:
    break;
  }

  k.set(current_k);
}

// === Anima Flow Implementation ===
// === Public

ANIMA_FLOW_T_IMPL
ANIMA_FLOW_C::AnimaFlow(IAnimaLeds<N_LED_UNITS> &anima_leds)
    : anima_leds(anima_leds)
{
  set_brightness(1.0f);
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
void ANIMA_FLOW_C::set_mask(uint8_t layer_i, const mask_array_t &mask)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.set(mask, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_mask_if_free(uint8_t layer_i, const mask_array_t &mask)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.set(mask, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::enable_mask_all(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.enable_all(true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::enable_mask_all_if_free(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.enable_all(false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::disable_mask_all(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.disable_all(true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::disable_mask_all_if_free(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.disable_all(false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::enable_mask_single(uint8_t layer_i, int i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.enable_single(i, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::enable_mask_single_if_free(uint8_t layer_i, int i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.enable_single(i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::disable_mask_single(uint8_t layer_i, int i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.disable_single(i, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::disable_mask_single_if_free(uint8_t layer_i, int i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.disable_single(i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::enable_mask_range(uint8_t layer_i, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.enable_range(start_i, end_i, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::enable_mask_range_if_free(uint8_t layer_i, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.enable_range(start_i, end_i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::disable_mask_range(uint8_t layer_i, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return;

  mask_control[layer_i].update([&](mask_control_t &m)
                               { m.disable_range(start_i, end_i, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::disable_mask_range_if_free(uint8_t layer_i, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return mask_control[layer_i].update([&](mask_control_t &m)
                                      { return m.disable_range(start_i, end_i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_colors(uint8_t layer_i, const rgb_array_t &rgb)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                                { c.set_colors(rgb, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_colors_if_free(uint8_t layer_i, const rgb_array_t &rgb)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_colors(rgb, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_solid(uint8_t layer_i, const CRGB &color)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                                { c.set_solid(color, true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_solid(uint8_t layer_i, const CRGB &color, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return;

  color_control[layer_i].update([&](color_control_t &c)
                                { c.set_solid(color, start_i, end_i, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_solid_if_free(uint8_t layer_i, const CRGB &color)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_solid(color, false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_solid_if_free(uint8_t layer_i, const CRGB &color, int start_i, int end_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return color_control[layer_i].update([&](color_control_t &c)
                                       { return c.set_solid(color, start_i, end_i, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_hue(uint8_t layer_i, uint8_t num_sectors, uint8_t units_per_color, uint8_t hue_shift)
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

  set_colors(layer_i, rgb);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::is_opacity_free(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.is_free(); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_opacity(uint8_t layer_i, float opac)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.set_k(opac, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_opacity_if_free(uint8_t layer_i, float opac)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.set_k(opac, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::fade_opacity(uint8_t layer_i, float opac_dest, millis_t duration_ms)
{
  if (layer_i >= N_LAYERS)
    return;

  if (duration_ms > 0)
    alpha_control[layer_i].update([&](k_control_t &a)
                                  { a.start_fade(opac_dest, duration_ms, true); });
  else
    set_opacity(layer_i, opac_dest);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::fade_opacity_if_free(uint8_t layer_i, float opac_dest, millis_t duration_ms)
{
  if (layer_i >= N_LAYERS)
    return false;

  if (duration_ms > 0)
    return alpha_control[layer_i].update([&](k_control_t &a)
                                         { return a.start_fade(opac_dest, duration_ms, false); });
  else
    return set_opacity_if_free(layer_i, opac_dest);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::show_layer(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    set_opacity(layer_i, 1.0);
  else
    fade_opacity(layer_i, 1.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::show_layer_if_free(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    return set_opacity_if_free(layer_i, 1.0);
  else
    return fade_opacity_if_free(layer_i, 1.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::hide_layer(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    set_opacity(layer_i, 0.0);
  else
    fade_opacity(layer_i, 0.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::hide_layer_if_free(uint8_t layer_i, millis_t duration_ms)
{
  if (duration_ms == 0)
    return set_opacity_if_free(layer_i, 0.0);
  else
    return fade_opacity_if_free(layer_i, 0.0, duration_ms);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::apply_opacity_envelope(uint8_t layer_i, const AnimaEnvelope &env)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.apply_envelope(env); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_opacity_envelope(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.start_envelope(true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_opacity_envelope(uint8_t layer_i, const AnimaEnvelope &env)
{
  if (layer_i >= N_LAYERS)
    return;

  alpha_control[layer_i].update([&](k_control_t &a)
                                { a.start_envelope(env, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_opacity_envelope_if_free(uint8_t layer_i)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.start_envelope(false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_opacity_envelope_if_free(uint8_t layer_i, const AnimaEnvelope &env)
{
  if (layer_i >= N_LAYERS)
    return false;

  return alpha_control[layer_i].update([&](k_control_t &a)
                                       { return a.start_envelope(env, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::set_brightness(float br)
{
  brightness_control.update([&](k_control_t &b)
                            { b.set_k(br, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::set_brightness_if_free(float br)
{
  return brightness_control.update([&](k_control_t &b)
                                   { return b.set_k(br, false); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::fade_brightness(float bright_dest, millis_t duration_ms)
{
  if (duration_ms > 0)
    brightness_control.update([&](k_control_t &b)
                              { b.start_fade(bright_dest, duration_ms, true); });
  else
    set_brightness(bright_dest);
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::fade_brightness_if_free(float bright_dest, millis_t duration_ms)
{
  if (duration_ms > 0)
    return brightness_control.update([&](k_control_t &b)
                                     { return b.start_fade(bright_dest, duration_ms, false); });
  else
    return set_brightness_if_free(bright_dest);
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::apply_brightness_envelope(const AnimaEnvelope &env, bool apply_dc_offset)
{
  if (apply_dc_offset)
  {
    AnimaEnvelope p = env;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    brightness_control.update([&](k_control_t &b)
                              { b.apply_envelope(p); });
    return;
  }

  brightness_control.update([&](k_control_t &b)
                            { b.apply_envelope(env); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_brightness_envelope()
{
  brightness_control.update([&](k_control_t &b)
                            { b.start_envelope(true); });
}

ANIMA_FLOW_T_IMPL
void ANIMA_FLOW_C::start_brightness_envelope(const AnimaEnvelope &env, bool apply_dc_offset)
{
  if (apply_dc_offset)
  {
    AnimaEnvelope p = env;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    brightness_control.update([&](k_control_t &b)
                              { b.start_envelope(p, true); });
    return;
  }

  brightness_control.update([&](k_control_t &b)
                            { b.start_envelope(env, true); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_brightness_envelope_if_free()
{
  return brightness_control.update([&](k_control_t &b)
                                   { return b.start_envelope(false); });
}

ANIMA_FLOW_T_IMPL
bool ANIMA_FLOW_C::start_brightness_envelope_if_free(const AnimaEnvelope &env, bool apply_dc_offset)

{
  if (apply_dc_offset)
  {
    AnimaEnvelope p = env;
    p.apply_dc_offset(p.get_amplitude(), get_bright_full_range());
    return brightness_control.update([&](k_control_t &b)
                                     { return b.start_envelope(p, false); });
  }

  return brightness_control.update([&](k_control_t &b)
                                   { return b.start_envelope(env, false); });
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
  float top_alpha = alpha[top_layer].get();

  while (top_layer > 0 && top_alpha <= 0.0f)
  {
    top_layer--;
    top_alpha = alpha[top_layer].get();
  }

  const mask_array_t &top_mask = mask[top_layer].get();
  bool top_mask_active = mask[top_layer].is_active();
  const rgb_array_t &top_rgb = color[top_layer].get_rgb();

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

    bool layer_mask_active = mask[layer_i].is_active();
    const mask_array_t &layer_mask = mask[layer_i].get();
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
        else
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], CRGB::Black, top_alpha);
    }
    else // masks for both blending layers active
    {
      for (size_t i = 0; i < N_LED_UNITS; i++)
        if (top_mask[i] && layer_mask[i])
          this->out_buf_leds.rgb[i] = AnimaFuncs::blend_colors(this->out_buf_leds.rgb[i], layer_rgb[i], top_alpha);
        else if (layer_mask[i])
          this->out_buf_leds.rgb[i] = layer_rgb[i];
    }

    top_alpha += layer_alpha * (1.0f - top_alpha);
    top_mask_active = layer_mask_active;

    if (layer_alpha >= 1.0f)
      break;
  }

  // processing resulting layer if not fully opaque
  if (top_alpha < 1.0f)
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
