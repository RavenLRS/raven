
#include "util/time.h"

#include "system.h"

static system_flag_e system_flags = 0;

system_flag_e system_get_flags(void)
{
    return system_flags;
}

system_flag_e system_add_flag(system_flag_e flag)
{
    system_flag_e old_flags = system_flags;
    system_flags |= flag;
    return old_flags;
}

system_flag_e system_remove_flag(system_flag_e flag)
{
    system_flag_e old_flags = system_flags;
    system_flags &= ~flag;
    return old_flags;
}

bool system_has_flag(system_flag_e flag)
{
    return system_flags & flag;
}
