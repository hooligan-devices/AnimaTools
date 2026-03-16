#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "internal/rtos-helpers.h"
#include "AnimaTypes.h"
#include "AnimaFuncs.h"

#define LAYER_0 0
#define BG_LAYER LAYER_0
#define LAYER_1 1
#define LAYER_2 2
#define LAYER_3 3
#define LAYER_4 4
#define LAYER_5 5
#define LAYER_6 6
#define LAYER_7 7
#define LAYER_8 8
#define LAYER_9 9

#define I_ANIMA_LEDS_T template <int N_LED_UNITS>
#define I_ANIMA_LEDS_T_IMPL template <int N_LED_UNITS>
#define I_ANIMA_LEDS_C IAnimaLeds<N_LED_UNITS>
#define ANIMA_LEDS_T template <uint8_t LED_PIN, int N_LED_UNITS, int N_LEDS_IN_UNIT = 1>
#define ANIMA_LEDS_T_IMPL template <uint8_t LED_PIN, int N_LED_UNITS, int N_LEDS_IN_UNIT>
#define ANIMA_LEDS_C AnimaLeds<LED_PIN, N_LED_UNITS, N_LEDS_IN_UNIT>

#define I_ANIMA_FLOW_T template <int N_LED_UNITS>
#define I_ANIMA_FLOW_T_IMPL template <int N_LED_UNITS>
#define I_ANIMA_FLOW_C IAnimaFlow<N_LED_UNITS>
#define ANIMA_FLOW_T template <int N_LED_UNITS, uint8_t N_LAYERS>
#define ANIMA_FLOW_T_IMPL template <int N_LED_UNITS, uint8_t N_LAYERS>
#define ANIMA_FLOW_C AnimaFlow<N_LED_UNITS, N_LAYERS>

// Main classes to create leds object and flow objects
ANIMA_LEDS_T
class AnimaLeds;

ANIMA_FLOW_T
class AnimaFlow;

// Interfaces for AnimaLeds and AnimaFlow, 
// used to pass as parameters with single template argument <N_LED_UNITS>
I_ANIMA_LEDS_T
class IAnimaLeds;

I_ANIMA_FLOW_T
class IAnimaFlow;

// Virtual part of IAnimaFlow, moved out of template class
class IAnimaFlow_
{
public:
    IAnimaFlow_() {};
    virtual void update();
};

I_ANIMA_FLOW_T_IMPL
class IAnimaFlow : IAnimaFlow_
{
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;

public:
    struct leds_t
    {
        rgb_array_t rgb;
        float brightness = 1.0f;
        leds_t();
    };

    leds_t out_buf_leds;

    IAnimaFlow() {};
    void update() override {};
};

I_ANIMA_LEDS_T_IMPL
class IAnimaLeds
{
public:
    // extra brightness kept for effects like pulsing/flickering
    const float BRIGHT_HEADROOM = 0.2f;
    const float BRIGHT_FULL_RANGE = 1.0f + BRIGHT_HEADROOM;

    const uint8_t BRIGHT_MIN_U8 = 30;
    const uint8_t BRIGHT_ABSOLUTE_MAX_U8 = UINT8_MAX;
    const uint8_t BRIGHT_NOMINAL_MAX_U8 = roundf((float)(BRIGHT_ABSOLUTE_MAX_U8) / BRIGHT_FULL_RANGE);

    IAnimaLeds() {};
    void connect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms = 0);
    void disconnect(millis_t transition_ms = 0);
    void disconnect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms = 0);
    bool is_connected(IAnimaFlow<N_LED_UNITS> &anima_flow);
    float get_bright_headroom() const;
    float get_bright_full_range() const;

protected:
    struct flow_params_t
    {
        IAnimaFlow<N_LED_UNITS> *main_flow = nullptr;
        IAnimaFlow<N_LED_UNITS> *last_flow = nullptr;
        millis_t transition_ms = 0;
        millis_t transition_start_ms = 0;
        bool is_transitioning = false;
    };

    struct flow_params_protected_t : public MutexProtected
    {
        MUTEX_PROTECTED_STRUCT(flow_params_t)
    };

    flow_params_protected_t flow_params;
};

ANIMA_LEDS_T_IMPL
class AnimaLeds : public I_ANIMA_LEDS_C
{
    using flow_params_t = typename IAnimaLeds<N_LED_UNITS>::flow_params_t;
    using leds_t = typename IAnimaFlow<N_LED_UNITS>::leds_t;
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;

    struct brightness_state_t
    {
        void setup(uint8_t br_min_u8, uint8_t br_nom_max_u8, uint8_t br_abs_max_u8);
        void update(float device_brightness, float on_off_fade_k);
        float get_device_brightness() const;
        uint8_t get_ready_u8(float br_flow) const;

    private:
        uint8_t br_abs_max_u8 = UINT8_MAX;
        uint8_t br_nom_max_u8 = UINT8_MAX;
        float min_device_br = 0.0f;

        float on_off_fade_k = 0.0f;
        float device_brightness = 0.0f;
        float device_br_scaled = 0.0f;
    };

    struct br_control_t
    {
        float get() const;
        void set(float br, millis_t fade_ms = 0);
        void on(millis_t fade_ms = 0);
        void off(millis_t fade_ms = 0);
        bool is_animating() const;
        void update(brightness_state_t &br_state);

    private:
        float on_off_fade_k = 0.0f;
        float device_brightness = 0.0f;
        bool is_animating_ = false;
        AnimaTypes::fade_t fader_global;
        AnimaTypes::fade_t fader_on_off;
    };

    struct br_control_protected_t : public MutexProtected
    {
        MUTEX_PROTECTED_STRUCT(br_control_t)
    };

    brightness_state_t brightness;
    br_control_protected_t brightness_control;

public:
    AnimaLeds();
    void update_and_show();
    void set_brightness(float br);
    void set_brightness(float br, millis_t fade_ms); 
    void on();
    void on(millis_t fade_ms);
    void off();
    void off(millis_t fade_ms);
    bool is_animating() const;

private:
    CRGB leds_FastLED[N_LED_UNITS * N_LEDS_IN_UNIT];
    bool is_handle_off_state = false;
};

ANIMA_FLOW_T_IMPL
class AnimaFlow : public I_ANIMA_FLOW_C
{
private:
    using MANAGER = AnimaTypes::MANAGER;
    // using leds_t = typename IAnimaFlow<N_LED_UNITS>::leds_t;
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;
    using mask_array_t = std::array<bool, N_LED_UNITS>;

    static constexpr millis_t FADE_DFLT_MS = 300;

    struct mask_state_t
    {
        mask_state_t();
        const mask_array_t &get() const;
        void set(const mask_array_t &mask);
        bool is_active() const;
        void set_active(bool active);

    private:
        mask_array_t mask;
        bool mask_is_active = false;
    };

    struct mask_control_t
    {
        mask_control_t();
        bool set(const mask_array_t &mask, bool terminate);
        bool enable_all(bool terminate);
        bool disable_all(bool terminate);
        bool enable_single(int i, bool terminate);
        bool disable_single(int i, bool terminate);
        bool enable_range(int start_i, int end_i, bool terminate);
        bool disable_range(int start_i, int end_i, bool terminate);
        void update(mask_state_t &m);

    private:
        AnimaTypes::manager_t owner;
        mask_array_t mask;
        bool mask_is_active = false;
    };

    struct mask_control_protected_t : public MutexProtected
    {
        MUTEX_PROTECTED_STRUCT(mask_control_t)
    };

    struct color_state_t
    {
        color_state_t();
        const rgb_array_t &get_rgb() const;
        void set_rgb(const rgb_array_t &rgb);

    private:
        rgb_array_t rgb;
    };

    struct color_control_t
    {
        color_control_t();
        bool set_colors(rgb_array_t rgb_arr, bool terminate);
        bool set_solid(CRGB color, bool terminate);
        bool set_solid(CRGB color, int start_i, int end_i, bool terminate);
        void update(color_state_t &l);

    private:
        AnimaTypes::manager_t owner;
        rgb_array_t rgb;
    };

    struct color_control_protected_t : public MutexProtected
    {
        MUTEX_PROTECTED_STRUCT(color_control_t)
    };

    struct k_state_t
    {
        void set(float k);
        float get() const;

    private:
        float k = 0.0f;
    };

    struct k_control_t
    {
        bool get_k() const;
        bool set_k(float k, bool terminate);
        bool start_fade(float k_dest, millis_t duration_ms, bool terminate);
        void apply_envelope(const AnimaEnvelope &env);
        bool start_envelope(bool terminate);
        bool start_envelope(const AnimaEnvelope &env, bool terminate);
        void stop_envelope();
        void terminate_envelope();
        bool is_free() const;
        void update(k_state_t &k);

    private:
        float current_k = 0.0f;
        AnimaTypes::manager_t owner;
        AnimaTypes::fade_t fader;
        AnimaTypes::envelope_runner_t envelope_runner;
    };

    struct k_control_protected_t : public MutexProtected
    {
        MUTEX_PROTECTED_STRUCT(k_control_t)
    };

    IAnimaLeds<N_LED_UNITS> &anima_leds;

    std::array<mask_state_t, N_LAYERS> mask;
    std::array<color_state_t, N_LAYERS> color;
    std::array<k_state_t, N_LAYERS> alpha;
    k_state_t brightness;

    std::array<mask_control_protected_t, N_LAYERS> mask_control;
    std::array<color_control_protected_t, N_LAYERS> color_control;
    std::array<k_control_protected_t, N_LAYERS> alpha_control;
    k_control_protected_t brightness_control;

public:
    AnimaFlow(IAnimaLeds<N_LED_UNITS> &anima_leds);
    void connect(millis_t transition_ms = 0);
    void disconnect(millis_t transition_ms = 0);
    bool is_connected();

    // == MASK MANAGING
    void set_mask(uint8_t layer_i, const std::array<bool, N_LED_UNITS> &mask);
    bool set_mask_if_free(uint8_t layer_i, const std::array<bool, N_LED_UNITS> &mask);
    void enable_mask_all(uint8_t layer_i);
    bool enable_mask_all_if_free(uint8_t layer_i);
    void disable_mask_all(uint8_t layer_i);
    bool disable_mask_all_if_free(uint8_t layer_i);
    void enable_mask_single(uint8_t layer_i, int i);
    bool enable_mask_single_if_free(uint8_t layer_i, int i);
    void disable_mask_single(uint8_t layer_i, int i);
    bool disable_mask_single_if_free(uint8_t layer_i, int i);
    void enable_mask_range(uint8_t layer_i, int start_i, int end_i);
    bool enable_mask_range_if_free(uint8_t layer_i, int start_i, int end_i);
    void disable_mask_range(uint8_t layer_i, int start_i, int end_i);
    bool disable_mask_range_if_free(uint8_t layer_i, int start_i, int end_i);

    // == COLOR MANAGING
    void set_colors(uint8_t layer_i, const std::array<CRGB, N_LED_UNITS> &rgb);
    bool set_colors_if_free(uint8_t layer_i, const std::array<CRGB, N_LED_UNITS> &rgb);
    void set_solid(uint8_t layer_i, const CRGB &color);
    void set_solid(uint8_t layer_i, const CRGB &color, int start_i, int end_i);
    bool set_solid_if_free(uint8_t layer_i, const CRGB &color);
    bool set_solid_if_free(uint8_t layer_i, const CRGB &color, int start_i, int end_i);
    // num_sectors = {1, 2, 3, 6} -> num of colors equally distanced on hue wheel.
    void set_hue(uint8_t layer_i, uint8_t num_sectors, uint8_t units_per_color = 1, uint8_t hue_shift = 0);
    
    // == OPACITY MANAGING
    bool is_opacity_free(uint8_t layer_i);
    void set_opacity(uint8_t layer_i, float opac);
    bool set_opacity_if_free(uint8_t layer_i, float opac);
    void fade_opacity(uint8_t layer_i, float opac_dest, millis_t duration_ms = FADE_DFLT_MS);
    bool fade_opacity_if_free(uint8_t layer_i, float opac_dest, millis_t duration_ms = FADE_DFLT_MS);
    void show_layer(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    bool show_layer_if_free(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    void hide_layer(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    bool hide_layer_if_free(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);

    void apply_opacity_envelope(uint8_t layer_i, const AnimaEnvelope &env);
    void start_opacity_envelope(uint8_t layer_i);
    void start_opacity_envelope(uint8_t layer_i, const AnimaEnvelope &env);
    bool start_opacity_envelope_if_free(uint8_t layer_i);
    bool start_opacity_envelope_if_free(uint8_t layer_i, const AnimaEnvelope &env);

    // == BRIGTNESS MANAGING
    void set_brightness(float br);
    bool set_brightness_if_free(float br);
    void fade_brightness(float bright_dest, millis_t duration_ms = FADE_DFLT_MS);
    bool fade_brightness_if_free(float bright_dest, millis_t duration_ms = FADE_DFLT_MS);

    void apply_brightness_envelope(const AnimaEnvelope &env, bool apply_dc_offset = false);
    void start_brightness_envelope();
    void start_brightness_envelope(const AnimaEnvelope &env, bool apply_dc_offset = false);
    bool start_brightness_envelope_if_free();
    bool start_brightness_envelope_if_free(const AnimaEnvelope &env, bool apply_dc_offset = false);
    
    int get_n_led_units() const;
    uint8_t get_n_layers() const;
    float get_bright_headroom() const;
    float get_bright_full_range() const;

protected:
    void update() override;

private:
    void make_output_leds();
};

#include "AnimaTools.tpp"
