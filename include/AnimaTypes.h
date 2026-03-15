#pragma once
#include <Arduino.h>
#include "AnimaFuncs.h"

using millis_t = decltype(millis());

namespace AnimaTypes
{
    struct manager_t;
    struct fade_t;
    struct envelope_t;
    struct envelope_runner_t;
}

using AnimaEnvelope = AnimaTypes::envelope_t;

namespace AnimaTypes
{
    static constexpr float S_CURVE_LINE = 1.0f;
    static constexpr float S_CURVE_SINE = 1.7f;

    enum class MANAGER
    {
        NONE = 0,
        SETTER,
        FADER,
        ENVELOPE
    };

    struct manager_t
    {
        void set(MANAGER o);
        void set_none();
        bool is_owner(MANAGER o) const;
        bool is_some() const;
        bool is_none() const;
        MANAGER get() const;
        void request(MANAGER o);
        bool is_requested() const;
        void apply_request();
        bool is_free() const;

    private:
        MANAGER owner = MANAGER::NONE;
        MANAGER requested = MANAGER::NONE;
    };

    struct fade_t
    {
        fade_t(float s_curve = S_CURVE_SINE);
        float get_k() const;
        void start(float k_from, float k_to, millis_t duration_ms);
        void terminate();
        bool terminate_if_cross(float cross_point);
        bool update();
        bool is_running() const;

    private:
        float current_k = 0.0f;
        float last_k = 0.0f;
        float k_from = 0.0f;
        float k_diff = 0.0f;
        millis_t duration_ms = 0;
        millis_t started_ms = 0;
        float s_curve = 1.0f;
        bool is_running_ = false;
    };

    struct envelope_t
    {
        uint8_t n_times = 0; // 0 means infinite
        float level_a = 0.0f;
        float level_b = 0.0f;
        float stop_level = 0.0f;
        bool stop_where_started = false;
        millis_t fade_a_ms = 0;
        millis_t fade_b_ms = 0;
        millis_t hold_a_ms = 0;
        millis_t hold_b_ms = 0;

        void apply_dc_offset(float offset);
        void apply_dc_offset(float offset, float limit_level);
        float get_amplitude() const;
        void make_timed(millis_t duration_ms, millis_t fade_a_ms, millis_t fade_b_ms);
        void make_breathing(millis_t period_ms, float range, float duty_factor = 0.5f, float stop_level = 1.0f);
    };

    struct envelope_runner_t
    {
        // n_times = 0 means infinite
        float get_level() const;
        void apply(const envelope_t &env);
        void start(float init_level);
        void start(const envelope_t &env, float init_level);
        void stop();
        void terminate();
        bool update(fade_t &fader);

    private:
        envelope_t envelope;
        float current_level = 0.0f;
        uint8_t n_times_cnt = 0;
        uint8_t phase_cnt = 0;
        bool is_running = false;
        bool stop_flag = false;
        millis_t timestamp = 0;
    };
}