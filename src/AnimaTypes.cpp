#include "AnimaTypes.h"

namespace AnimaTypes
{
    // =================
    // === manager_t ===
    void manager_t::set(MANAGER o) { owner = o; }
    void manager_t::set_none() { owner = MANAGER::NONE; }
    bool manager_t::is_owner(MANAGER o) const { return (owner == o); }
    bool manager_t::is_some() const { return (owner != MANAGER::NONE); }
    bool manager_t::is_none() const { return (owner == MANAGER::NONE); }
    MANAGER manager_t::get() const { return owner; }
    void manager_t::request(MANAGER o) { requested = o; }
    bool manager_t::is_requested() const { return (requested != MANAGER::NONE); }
    void manager_t::apply_request()
    {
        if (requested == MANAGER::NONE)
            return;
        owner = requested;
        requested = MANAGER::NONE;
    }
    bool manager_t::is_free() const { return (owner == MANAGER::NONE && requested == MANAGER::NONE); }

    // ==============
    // === fade_t ===
    fade_t::fade_t(float s_curve) : s_curve(s_curve) {}

    float fade_t::get_k() const { return current_k; }

    void fade_t::start(float k_from, float k_to, millis_t duration_ms)
    {
        current_k = k_from;
        this->k_from = k_from;
        k_diff = k_to - k_from;
        this->duration_ms = duration_ms;
        started_ms = millis();
        is_running_ = true;
    }

    void fade_t::terminate() { is_running_ = false; }

    bool fade_t::terminate_if_cross(float cross_point)
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

    bool fade_t::update()
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

    bool fade_t::is_running() const { return is_running_; }

    // ==================
    // === envelope_t ===
    void envelope_t::apply_dc_offset(float offset)
    {
        level_a += offset;
        level_b += offset;
    }

    void envelope_t::apply_dc_offset(float offset, float limit_level)
    {
        float max_level = max(level_a, level_b);
        offset = min(offset, limit_level - max_level);
        apply_dc_offset(offset);
    }

    float envelope_t::get_amplitude() const { return abs(level_a - level_b) / 2.0f; }

    void envelope_t::make_timed(millis_t duration_ms, millis_t fade_a_ms, millis_t fade_b_ms)
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

    void envelope_t::make_breathing(millis_t period_ms, float range, float duty_factor, float stop_level)
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

    // =========================
    // === envelope_runner_t ===
    float envelope_runner_t::get_level() const { return current_level; }

    void envelope_runner_t::apply(const envelope_t &env)
    {
        envelope = env;
        n_times_cnt = 0;
    }

    void envelope_runner_t::start(float init_level)
    {
        current_level = init_level;
        n_times_cnt = 0;
        phase_cnt = 0;
        stop_flag = false;
        is_running = true;
    }

    void envelope_runner_t::start(const envelope_t &env, float init_level)
    {
        envelope_t e;
        if (env.stop_where_started)
        {
            e = env;
            e.stop_level = init_level;
            apply(e);
        }
        else
        {
            apply(env);
        }

        start(init_level);
    }

    void envelope_runner_t::stop() { stop_flag = true; }
    void envelope_runner_t::terminate() { is_running = false; }

    bool envelope_runner_t::update(fade_t &fader)
    {
        if (!is_running)
            return false;

        bool is_change_final_fade = false;
        envelope_t &env = envelope;

        switch (phase_cnt)
        {
        case 0:
            if (n_times_cnt == 0)
                timestamp = millis();

            // if k_dest is out of range (from, to) - process final fade in differently
            if (stop_flag)
            {
                if (env.stop_level > env.level_a && env.level_a > env.level_b)
                    is_change_final_fade = true;
                else if (env.stop_level < env.level_a && env.level_a < env.level_b)
                    is_change_final_fade = true;
            }

            if (is_change_final_fade)
                fader.start(current_level, env.stop_level, env.fade_a_ms);
            else
                fader.start(current_level, env.level_a, env.fade_a_ms);

            phase_cnt++;

        case 1: // fade from k_from (or current_k) to level_a
            if (fader.update())
                current_level = fader.get_k();

            if (fader.is_running())
                break;

            timestamp += env.fade_a_ms;
            phase_cnt++;

        case 2:
            if ((millis() - timestamp) < env.hold_a_ms)
                break;

            phase_cnt++;
            timestamp += env.hold_a_ms;

        case 3: // fade from level_a to level_b
            if (env.n_times == (n_times_cnt + 1) && env.n_times != 0)
                stop_flag = true;

            // if k_dest is out of range (from, to) - process fade out differently on stop
            if (stop_flag)
            {
                if (env.stop_level < env.level_b && env.level_b < env.level_a)
                    is_change_final_fade = true;
                else if (env.stop_level > env.level_b && env.level_b > env.level_a)
                    is_change_final_fade = true;
            }

            if (is_change_final_fade)
                fader.start(current_level, env.stop_level, env.fade_b_ms);
            else
                fader.start(current_level, env.level_b, env.fade_b_ms);

            phase_cnt++;

        case 4:
            if (fader.update())
                current_level = fader.get_k();

            if (fader.is_running())
                break;

            timestamp += env.fade_b_ms;
            phase_cnt++;

        case 5:
            if ((millis() - timestamp) < env.hold_b_ms)
                break;

            phase_cnt = 0;
            n_times_cnt++;
            timestamp += env.hold_b_ms;
            break;

        default:
            break;
        }

        if (stop_flag)
            if (fader.terminate_if_cross(env.stop_level))
                is_running = false;

        return is_running;
    }
}
