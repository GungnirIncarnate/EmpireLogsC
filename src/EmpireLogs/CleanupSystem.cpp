#include "EmpireLogs/CleanupSystem.h"

#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

namespace EmpireLogs
{
    int CleanupSystem::Run(SharedState& shared_state)
    {
        const int removed = shared_state.RemoveEmptyGuilds();
        if (removed > 0)
        {
            PCL_Log("GUILD_CLEANUP | removed_empty_guilds={}", removed);
        }
        return removed;
    }
}
