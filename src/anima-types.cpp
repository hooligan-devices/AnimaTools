#include "anima-types.h"

namespace AnimaTypes
{
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
    bool manager_t::is_idle() { return (owner == MANAGER::NONE && requested == MANAGER::NONE); }


    
}
