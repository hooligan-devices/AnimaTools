#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "internal/mutex-struct.h"
#include "anima-funcs.h"
#include "anima-types.h"

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

#define ANIMA_LEDS_T template <uint8_t LED_PIN, int N_LED_UNITS, int N_LEDS_IN_UNIT = 1>
#define ANIMA_LEDS_T_IMPL template <uint8_t LED_PIN, int N_LED_UNITS, int N_LEDS_IN_UNIT>
#define ANIMA_LEDS_C AnimaLeds<LED_PIN, N_LED_UNITS, N_LEDS_IN_UNIT>

#define ANIMA_FLOW_T template <int N_LED_UNITS, uint8_t N_LAYERS>
#define ANIMA_FLOW_T_IMPL template <int N_LED_UNITS, uint8_t N_LAYERS>
#define ANIMA_FLOW_C AnimaFlow<N_LED_UNITS, N_LAYERS>

ANIMA_LEDS_T
class AnimaLeds;

ANIMA_FLOW_T
class AnimaFlow;

class IAnimaFlow_
{
public:
    IAnimaFlow_() {};
    virtual void update();
};

template <int N_LED_UNITS>
class IAnimaFlow : IAnimaFlow_
{
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;

public:
    struct leds_t
    {
        rgb_array_t rgb;
        float brightness = 1.0f;
        leds_t() { rgb.fill(CRGB::Black); };
    };

    leds_t out_buf_leds;

    IAnimaFlow() {};
    void update() override {};
};

template <int N_LED_UNITS>
class IAnimaLeds
{
public:
    // extra brightness kept for effects like pulsing/flickering
    const float BRIGHT_HEADROOM = 0.2f;
    const float BRIGHT_FULL_RANGE = 1.0f + BRIGHT_HEADROOM;

    const uint8_t BRIGHT_MIN_U8 = 30;
    const uint8_t BRIGHT_ABSOLUTE_MAX_U8 = 255;
    const uint8_t BRIGHT_NOMINAL_MAX_U8 = roundf((float)(BRIGHT_ABSOLUTE_MAX_U8) / BRIGHT_FULL_RANGE);

    IAnimaLeds() {};
    void connect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms = 0);
    void disconnect(millis_t transition_ms = 0);
    void disconnect(IAnimaFlow<N_LED_UNITS> &anima_flow, millis_t transition_ms = 0);
    bool is_connected(IAnimaFlow<N_LED_UNITS> &anima_flow);
    inline float get_bright_headroom() const;
    inline float get_bright_full_range() const;

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
class AnimaLeds : public IAnimaLeds<N_LED_UNITS>
{
    using flow_params_t = typename IAnimaLeds<N_LED_UNITS>::flow_params_t;
    using leds_t = typename IAnimaFlow<N_LED_UNITS>::leds_t;
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;

    struct brightness_state_t
    {
        inline void setup(uint8_t br_min_u8, uint8_t br_nom_max_u8, uint8_t br_abs_max_u8)
        {
            this->br_abs_max_u8 = br_abs_max_u8;
            this->br_nom_max_u8 = br_nom_max_u8;
            min_device_br = (float)br_min_u8 / (float)br_nom_max_u8;
        }

        inline void update(float device_brightness, float on_off_fade_k)
        {
            this->device_brightness = device_brightness;
            device_br_scaled = device_brightness * (1.0f - min_device_br) + min_device_br;
            this->on_off_fade_k = on_off_fade_k;
        }

        inline float get_device_brightness() const { return device_brightness; }
                 
        inline uint8_t get_ready_u8(float br_flow) const
        {
            int br = br_flow * device_br_scaled * on_off_fade_k * br_nom_max_u8;
            br = constrain(br, 0, br_abs_max_u8);
            return uint8_t(br);
        }

    private:
        uint8_t br_abs_max_u8 = 255;
        uint8_t br_nom_max_u8 = 255;
        float min_device_br = 0.0f;

        float on_off_fade_k = 0.0f;
        float device_brightness = 0.0f;
        float device_br_scaled = 0.0f;
    };

    struct br_control_t
    {
        inline float get() const { return device_brightness; }
        inline void set(float br, millis_t fade_ms = 0)
        {
            br = constrain(br, 0.0f, 1.0f);
            fader_global.start(device_brightness, br, fade_ms);
            is_animating_ = true;
        }

        inline void on(millis_t fade_ms = 0)
        {
            fader_on_off.start(on_off_fade_k, 1.0f, fade_ms);
            is_animating_ = true;
        }

        inline void off(millis_t fade_ms = 0)
        {
            fader_on_off.start(on_off_fade_k, 0.0f, fade_ms);
            is_animating_ = true;
        }

        bool is_animating() const { return is_animating_; }

        inline void update(brightness_state_t &br_state)
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

    void set_brightness(float br)
    {
        brightness_control.update([&](br_control_t &b)
                                  { b.set(br); });
    }

    void set_brightness(float br, millis_t fade_ms)
    {
        brightness_control.update([&](br_control_t &b)
                                  { b.set(br, fade_ms); });
    }

    void on()
    {
        brightness_control.update([&](br_control_t &b)
                                  { b.on(); });
    }

    void on(millis_t fade_ms)
    {
        brightness_control.update([&](br_control_t &b)
                                  { b.on(fade_ms); });
    }

    void off()
    {
        is_handle_off_state = true;
        brightness_control.update([&](br_control_t &b)
                                  { b.off(); });
    }

    void off(millis_t fade_ms)
    {
        is_handle_off_state = true;
        brightness_control.update([&](br_control_t &br)
                                  { br.off(fade_ms); });
    }

    bool is_animating() const
    {
        bool is_bright_running = brightness_control.read([](const br_control_t &br)
                                                         { return br.is_animating(); });

        bool is_transitioning = this->flow_params.read([](const flow_params_t &fp)
                                                       { return fp.is_transitioning; });

        return is_bright_running || is_transitioning;
    }

private:
    CRGB leds_FastLED[N_LED_UNITS * N_LEDS_IN_UNIT];
    bool is_handle_off_state = false;
};

ANIMA_FLOW_T_IMPL
class AnimaFlow : public IAnimaFlow<N_LED_UNITS>
{
private:
    using MANAGER = AnimaTypes::MANAGER;
    using leds_t = typename IAnimaFlow<N_LED_UNITS>::leds_t;
    using rgb_array_t = std::array<CRGB, N_LED_UNITS>;
    using mask_array_t = std::array<bool, N_LED_UNITS>;

    static constexpr millis_t FADE_DFLT_MS = 300;

    struct mask_state_t
    {
        mask_state_t() { mask.fill(true); }
        inline const mask_array_t &get() const { return mask; }
        inline void set(const mask_array_t &mask) { this->mask = mask; }
        inline bool is_active() const { return mask_is_active; }
        inline void set_active(bool active) { mask_is_active = active; }

    private:
        mask_array_t mask;
        bool mask_is_active = false;
    };

    struct mask_control_t
    {
        mask_control_t() { mask.fill(true); }

        inline bool enable_all(bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            // mask not used on updates when mask_is_active == false
            // but needs to set anyway to store state to use selective setters afterwards (single, range)
            mask.fill(true); 
            this->mask_is_active = false;
            return true;
        }

        inline bool disable_all(bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            mask.fill(false);
            this->mask_is_active = true;
            return true;
        }

        inline bool set(const mask_array_t &mask, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            this->mask = mask;
            this->mask_is_active = true;
            return true;
        }

        inline bool enable_single(int i, bool terminate)
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

        inline bool disable_single(int i, bool terminate)
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

        inline bool enable_range(int start_i, int end_i, bool terminate)
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

        inline bool disable_range(int start_i, int end_i, bool terminate)
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

        inline void update(mask_state_t &m)
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
        color_state_t() { rgb.fill(CRGB::Black); }
        inline const rgb_array_t &get_rgb() const { return rgb; }
        inline void set_rgb(const rgb_array_t &rgb) { this->rgb = rgb; }

    private:
        rgb_array_t rgb;
    };

    struct color_control_t
    {
        color_control_t() { rgb.fill(CRGB::Black); }

        inline bool set_colors(rgb_array_t rgb_arr, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            rgb = rgb_arr;
            return true;
        }

        inline bool set_solid(CRGB color, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            rgb.fill(color);
            return true;
        }

        inline bool set_solid(CRGB color, int start_i, int end_i, bool terminate)
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

        inline void update(color_state_t &l)
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
        inline void set(float k) { this->k = k; }
        inline float get() const { return k; }

    private:
        float k = 0.0f;
    };

    struct k_control_t
    {
        inline bool get_k() const { return current_k; }

        inline bool set_k(float k, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::SETTER);
            current_k = k;
            return true;
        }

        inline bool start_fade(float k_dest, millis_t duration_ms, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::FADER);
            fader.start(current_k, k_dest, duration_ms);
            return true;
        }

        inline void apply_periodic(const AnimaPeriodic &p) { periodic_runner.apply(p); }

        inline bool start_periodic(bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::PERIODIC);
            periodic_runner.start(current_k);
            return true;
        }

        inline bool start_periodic(const AnimaPeriodic &p, bool terminate)
        {
            if (owner.is_some() && !terminate)
                return false;

            owner.request(MANAGER::PERIODIC);
            periodic_runner.start(p, current_k);
            return true;
        }

        inline void stop_periodic() { periodic_runner.stop(); }
        inline void terminate_periodic() { periodic_runner.terminate(); }
        inline bool is_idle() { return owner.is_idle(); }

        inline void update(k_state_t &k)
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
            case MANAGER::PERIODIC:
                if (periodic_runner.update(fader))
                    current_k = periodic_runner.get_level();
                else
                    owner.set_none();
                break;
            default:
                break;
            }

            k.set(current_k);
        }

    private:
        float current_k = 0.0f;
        AnimaTypes::manager_t owner;
        AnimaTypes::fade_t fader;
        AnimaTypes::periodic_runner_t periodic_runner;
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

    // == HUE MANAGING
    // num_sectors = {1, 2, 3, 6} -> num of colors equally distanced on hue wheel.
    void set_hue_now(uint8_t layer_i, uint8_t num_sectors, uint8_t units_per_color = 1, uint8_t hue_shift = 0);

    // == MASK MANAGING
    bool set_mask(uint8_t layer_i, const mask_array_t &mask);
    void set_mask_now(uint8_t layer_i, const mask_array_t &mask);

    // == COLOR MANAGING
    bool set_colors(uint8_t layer_i, const rgb_array_t &rgb);
    void set_colors_now(uint8_t layer_i, const rgb_array_t &rgb);
    bool set_solid(uint8_t layer_i, const CRGB &color);
    bool set_solid(uint8_t layer_i, const CRGB &color, int start_i, int end_i);
    void set_solid_now(uint8_t layer_i, const CRGB &color);
    void set_solid_now(uint8_t layer_i, const CRGB &color, int start_i, int end_i);

    // == OPACITY MANAGING
    bool is_opacity_idle(uint8_t layer_i);
    bool set_opacity(uint8_t layer_i, float opac);
    void set_opacity_now(uint8_t layer_i, float opac);
    bool fade_opacity(uint8_t layer_i, float opac_dest, millis_t duration_ms = FADE_DFLT_MS);
    void fade_opacity_now(uint8_t layer_i, float opac_dest, millis_t duration_ms = FADE_DFLT_MS);
    bool show_layer(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    void show_layer_now(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    bool hide_layer(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);
    void hide_layer_now(uint8_t layer_i, millis_t duration_ms = FADE_DFLT_MS);

    void apply_periodic_opacity(uint8_t layer_i, const AnimaPeriodic &periodic);
    bool start_periodic_opacity(uint8_t layer_i);
    bool start_periodic_opacity(uint8_t layer_i, const AnimaPeriodic &periodic);
    void start_periodic_opacity_now(uint8_t layer_i);
    void start_periodic_opacity_now(uint8_t layer_i, const AnimaPeriodic &periodic);

    // == BRIGTNESS MANAGING
    bool set_brightness(float br);
    void set_brightness_now(float br);
    bool fade_brightness(float bright_dest, millis_t duration_ms = FADE_DFLT_MS);
    void fade_brightness_now(float bright_dest, millis_t duration_ms = FADE_DFLT_MS);

    void apply_periodic_brightness(const AnimaPeriodic &periodic, bool apply_dc_offset = false);
    bool start_periodic_brightness();
    bool start_periodic_brightness(const AnimaPeriodic &periodic, bool apply_dc_offset = false);
    void start_periodic_brightness_now();
    void start_periodic_brightness_now(const AnimaPeriodic &periodic, bool apply_dc_offset = false);

    inline int get_n_led_units() const;
    inline uint8_t get_n_layers() const;
    inline float get_bright_headroom() const;
    inline float get_bright_full_range() const;

protected:
    void update() override;

private:
    void make_output_leds();
};

#include "anima-tools.tpp"
