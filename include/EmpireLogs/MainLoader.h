#pragma once

#include "EmpireLogs/BuildTracker.h"
#include "EmpireLogs/DismantleTracker.h"
#include "EmpireLogs/JoinTracker.h"
#include "EmpireLogs/SharedState.h"

namespace EmpireLogs
{
    class MainLoader
    {
    public:
        MainLoader();
        void Initialize();

    private:
        SharedState m_shared_state{};
        DismantleTracker m_dismantle_tracker;
        BuildTracker m_build_tracker;
        JoinTracker m_join_tracker;
    };
}
