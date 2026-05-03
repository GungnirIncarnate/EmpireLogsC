#include "EmpireLogs/MainLoader.h"

#include "Logger/Logger.h"

#include <Helpers/String.hpp>
#include <UE4SSProgram.hpp>

namespace EmpireLogs
{
    MainLoader::MainLoader()
        : m_dismantle_tracker(m_shared_state)
        , m_build_tracker(m_shared_state)
        , m_join_tracker(m_shared_state)
    {
    }

    void MainLoader::Initialize()
    {
        const auto working_dir = std::filesystem::path{RC::StringType{RC::UE4SSProgram::get_program().get_working_directory()}};
        m_shared_state.SetupPaths(working_dir);

        PCL_Log("EmpireLogs MainLoader starting.");
        PCL_Log("Log path: {}", RC::ensure_str(m_shared_state.GetLogPath().string()));

        m_shared_state.LoadLog();
        m_shared_state.SeedFromGuildExport();

        m_dismantle_tracker.InstallHook();
        m_build_tracker.InstallHook();
        m_join_tracker.InstallHook();

        PCL_Log("Hooks registered - EmpireLogs running.");
    }
}
