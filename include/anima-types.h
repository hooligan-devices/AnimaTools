#pragma once
#include <Arduino.h>
#include "anima-funcs.h"

using millis_t = decltype(millis());

namespace AnimaTypes
{
    struct manager_t;
    struct fade_t;
    struct Periodic;
    struct timed_t;
}

using AnimaPeriodic = AnimaTypes::Periodic;

namespace AnimaTypes
{
    static constexpr float S_CURVE_LINE = 1.0f;
    static constexpr float S_CURVE_SINE = 1.7f;

    enum class MANAGER
    {
        NONE = 0,
        SETTER,
        FADER,
        PERIODIC
    };

    struct manager_t
    {
        inline void set(MANAGER o);
        inline void set_none();
        inline bool is_owner(MANAGER o) const;
        inline bool is_some() const;
        inline bool is_none() const;
        inline MANAGER get() const;
        inline void request(MANAGER o);
        inline bool is_requested() const;
        inline void apply_request();
        inline bool is_idle();

    private:
        MANAGER owner = MANAGER::NONE;
        MANAGER requested = MANAGER::NONE;
    };

    struct fade_t
    {
        fade_t(float s_curve = S_CURVE_SINE) : s_curve(s_curve) {}
        inline float get_k() const { return current_k; }

        inline void start(float k_from, float k_to, millis_t duration_ms)
        {
            current_k = k_from;
            this->k_from = k_from;
            k_diff = k_to - k_from;
            this->duration_ms = duration_ms;
            started_ms = millis();
            is_running_ = true;
        }

        inline void terminate() { is_running_ = false; }

        inline bool terminate_if_cross(float cross_point)
        {
            if (!is_running_)
                return false;

            if (current_k >= cross_point && last_k <= cross_point)
                is_running_ = false;
            else if (current_k <= cross_point && last_k >= cross_point)
                is_running_ = false;

            if (is_running_)
                return false;

            current_k = cross_point;
            return true;
        }

        inline bool update()
        {
            bool is_updated = false;

            if (!is_running_)
                return is_updated;

            millis_t elapsed = millis() - started_ms;
            float fade_scale = 1.0f;

            if (elapsed <= duration_ms && duration_ms > 0)
            {
                fade_scale = (float)elapsed / duration_ms;
                fade_scale = constrain(fade_scale, 0.0, 1.0); // just in case
                if (s_curve != 1.0f)
                    fade_scale = AnimaFuncs::to_s_curve(fade_scale, s_curve);
            }

            if (elapsed >= duration_ms)
                is_running_ = false;

            last_k = current_k;
            current_k = k_from + k_diff * fade_scale;
            is_updated = true;

            return is_updated;
        }

        inline bool is_running() const { return is_running_; }

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

    struct Periodic
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

        inline void apply_dc_offset(float offset)
        {
            level_a += offset;
            level_b += offset;
        }

        inline void apply_dc_offset(float offset, float limit_level)
        {
            float max_level = max(level_a, level_b);
            offset = min(offset, limit_level - max_level);
            apply_dc_offset(offset);
        }

        inline float get_amplitude() const { return abs(level_a - level_b) / 2.0f; }

        inline void make_timed(millis_t duration_ms, millis_t fade_a_ms, millis_t fade_b_ms)
        {
            n_times = 1;
            level_a = 1.0f;
            level_b = 0.0f;
            this->fade_a_ms = fade_a_ms;
            this->fade_b_ms = fade_b_ms;
            hold_a_ms = duration_ms;
            hold_b_ms = 0;
            stop_where_started = false;
            stop_level = 0.0f;
        }

        inline void make_breathing(millis_t period_ms, float range, float duty_factor, float stop_level)
        {
            n_times = 0; // 0 means infinite
            level_a = 1.0f;
            level_b = 1.0f - range;
            fade_a_ms = period_ms * duty_factor;
            fade_b_ms = period_ms * (1.0f - duty_factor);
            hold_a_ms = 0;
            hold_b_ms = 0;
            stop_where_started = false;
            this->stop_level = stop_level;
        }

        // around 1.0f dc
        inline void make_breathing(millis_t period_ms, float range, float duty_factor)
        {
            make_breathing(period_ms, range, duty_factor, 1.0f);
        }

        // 0.5 duty factor, around 1.0f dc
        inline void make_breathing(millis_t period_ms, float range)
        {
            make_breathing(period_ms, range, 0.5f, 1.0f);
        }
    };

    struct periodic_runner_t
    {
        // n_times = 0 means infinite
        inline float get_level() const { return current_level; }

        inline void apply(const Periodic &p)
        {
            periodic = p;
            n_times_cnt = 0;
        }

        inline void start(float init_level)
        {
            current_level = init_level;
            n_times_cnt = 0;
            phase_cnt = 0;
            stop_flag = false;
            is_running = true;
        }

        inline void start(const Periodic &p, float init_level)
        {
            Periodic p_;
            if (p.stop_where_started)
            {
                p_ = p;
                p_.stop_level = init_level;
                apply(p_);
            }
            else
            {
                apply(p);
            }

            start(init_level);
        }

        inline void stop() { stop_flag = true; }
        inline void terminate() { is_running = false; }

        inline bool update(fade_t &fader)
        {
            if (!is_running)
                return false;

            bool is_change_final_fade = false;
            Periodic &p = periodic;

            switch (phase_cnt)
            {
            case 0:
                if (n_times_cnt == 0)
                    timestamp = millis();

                // if k_dest is out of range (from, to) - process final fade in differently
                if (stop_flag)
                {
                    if (p.stop_level > p.level_a && p.level_a > p.level_b)
                        is_change_final_fade = true;
                    else if (p.stop_level < p.level_a && p.level_a < p.level_b)
                        is_change_final_fade = true;
                }

                if (is_change_final_fade)
                    fader.start(current_level, p.stop_level, p.fade_a_ms);
                else
                    fader.start(current_level, p.level_a, p.fade_a_ms);

                phase_cnt++;

            case 1: // fade from k_from (or current_k) to level_a
                if (fader.update())
                    current_level = fader.get_k();

                if (fader.is_running())
                    break;

                timestamp += p.fade_a_ms;
                phase_cnt++;

            case 2:
                if ((millis() - timestamp) < p.hold_a_ms)
                    break;

                phase_cnt++;
                timestamp += p.hold_a_ms;

            case 3: // fade from level_a to level_b
                if (p.n_times == (n_times_cnt + 1) && p.n_times != 0)
                    stop_flag = true;

                // if k_dest is out of range (from, to) - process fade out differently on stop
                if (stop_flag)
                {
                    if (p.stop_level < p.level_b && p.level_b < p.level_a)
                        is_change_final_fade = true;
                    else if (p.stop_level > p.level_b && p.level_b > p.level_a)
                        is_change_final_fade = true;
                }

                if (is_change_final_fade)
                    fader.start(current_level, p.stop_level, p.fade_b_ms);
                else
                    fader.start(current_level, p.level_b, p.fade_b_ms);

                phase_cnt++;

            case 4:
                if (fader.update())
                    current_level = fader.get_k();

                if (fader.is_running())
                    break;

                timestamp += p.fade_b_ms;
                phase_cnt++;

            case 5:
                if ((millis() - timestamp) < p.hold_b_ms)
                    break;

                phase_cnt = 0;
                n_times_cnt++;
                timestamp += p.hold_b_ms;
                break;

            default:
                break;
            }

            if (stop_flag)
                if (fader.terminate_if_cross(p.stop_level))
                    is_running = false;

            return is_running;
        }

    private:
        Periodic periodic;
        float current_level = 0.0f;
        uint8_t n_times_cnt = 0;
        uint8_t phase_cnt = 0;
        bool is_running = false;
        bool stop_flag = false;
        millis_t timestamp = 0;
    };
}