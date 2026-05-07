#pragma once

namespace EmpireLogs
{
    class SharedState;

    class CleanupSystem
    {
    public:
        // Removes all guild entries with empty member lists.
        // Must be called while holding SharedState's mutex.
        // Returns the number of guilds removed.
        static int Run(SharedState& shared_state);
    };
}
