#include "EmpireLogs/BuildTracker.h"

#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <mutex>
#include <vector>

namespace EmpireLogs
{
    namespace
    {
        struct CompleteBuildParams
        {
            RC::Unreal::UObject* MapObjectModel{};
        };
    }

    BuildTracker::BuildTracker(SharedState& shared_state)
        : m_shared_state(shared_state)
    {
    }

    void BuildTracker::InstallHook()
    {
        m_hook_ids = RC::Unreal::UObjectGlobals::RegisterHook(
            STR("/Script/Pal.PalPlayerRecordData:OnCompleteBuild_ServerInternal"),
            &BuildTracker::OnHook,
            nullptr,
            this);

        PCL_Log("Hook [OnCompleteBuild_ServerInternal] IDs: ({}, {})", m_hook_ids.first, m_hook_ids.second);
    }

    void BuildTracker::OnHook(RC::Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        if (auto* self = static_cast<BuildTracker*>(custom_data))
        {
            self->Handle(context);
        }
    }

    void BuildTracker::Handle(RC::Unreal::UnrealScriptFunctionCallableContext& context)
    {
        // Skip processing during server startup to avoid reading uninitialized guild memory.
        // OnCompleteBuild_ServerInternal can fire during save restoration before guild
        // structures are fully initialized. Only process after a real player has joined.
        if (!m_shared_state.IsServerReady())
        {
            return;
        }

        const auto& params = context.GetParams<CompleteBuildParams>();
        (void)params;

        std::vector<RC::Unreal::UObject*> guild_objects;
        RC::Unreal::UObjectGlobals::FindAllOf(STR("PalGroupGuildBase"), guild_objects);
        if (guild_objects.empty())
        {
            return;
        }

        int changed_guilds = 0;
        {
            std::lock_guard<std::mutex> lock{m_shared_state.GetMutex()};

            for (auto* guild : guild_objects)
            {
                if (!guild)
                {
                    continue;
                }

                const auto guild_id = m_shared_state.ResolveGuildId(guild);
                if (guild_id.empty())
                {
                    continue;
                }

                const auto guild_name = m_shared_state.ResolveGuildName(guild);
                const auto live_count = m_shared_state.QueryLiveCampCount(guild);
                m_shared_state.EnsureGuild(guild_id, guild_name);
                const auto old_count = m_shared_state.GetCampCount(guild_id);
                if (old_count == live_count)
                {
                    continue;
                }

                m_shared_state.SetCampCount(guild_id, live_count);
                ++changed_guilds;
            }

            if (changed_guilds > 0)
            {
                m_shared_state.WriteLog();
            }
        }

        if (changed_guilds <= 0)
        {
            return;
        }
    }
}
