#include "EmpireLogs/GuildMemberUpdateTracker.h"

#include "EmpireLogs/CleanupSystem.h"
#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <mutex>
#include <vector>

namespace EmpireLogs
{
    namespace
    {
        auto sync_guild_members(SharedState& shared_state, RC::Unreal::UObject* guild) -> bool
        {
            if (!guild)
            {
                return false;
            }

            const auto guild_id = shared_state.ResolveGuildId(guild);
            if (guild_id.empty())
            {
                return false;
            }

            const auto guild_name = shared_state.ResolveGuildName(guild);
            const auto live_members = shared_state.QueryLiveMembers(guild);

            shared_state.EnsureGuild(guild_id, guild_name);
            return shared_state.SyncGuildMembersIfChanged(guild_id, live_members);
        }
    }

    GuildMemberUpdateTracker::GuildMemberUpdateTracker(SharedState& shared_state)
        : m_shared_state(shared_state)
    {
    }

    void GuildMemberUpdateTracker::InstallHook()
    {
        m_hook_ids = RC::Unreal::UObjectGlobals::RegisterHook(
            STR("/Script/Pal.PalPlayerState:OnUpdatePlayerInfoInGuildBelongTo"),
            &GuildMemberUpdateTracker::OnHook,
            nullptr,
            this);

        PCL_Log("Hook [OnUpdatePlayerInfoInGuildBelongTo] IDs: ({}, {})", m_hook_ids.first, m_hook_ids.second);
    }

    void GuildMemberUpdateTracker::OnHook(RC::Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        if (auto* self = static_cast<GuildMemberUpdateTracker*>(custom_data))
        {
            self->Handle(context);
        }
    }

    void GuildMemberUpdateTracker::Handle(RC::Unreal::UnrealScriptFunctionCallableContext& context)
    {
        auto* player_state = context.Context;
        if (!player_state)
        {
            return;
        }

        std::vector<RC::Unreal::UObject*> guild_objects;
        RC::Unreal::UObjectGlobals::FindAllOf(STR("PalGroupGuildBase"), guild_objects);

        int changed_guilds = 0;
        {
            std::lock_guard<std::mutex> lock{m_shared_state.GetMutex()};

            for (auto* guild : guild_objects)
            {
                if (sync_guild_members(m_shared_state, guild))
                {
                    ++changed_guilds;
                }
            }

            if (changed_guilds > 0)
            {
                CleanupSystem::Run(m_shared_state);
                m_shared_state.WriteLog();
            }
        }

        if (changed_guilds > 0)
        {
            PCL_Log("GUILD_MEMBER_UPDATE | changed_guilds={}", changed_guilds);
        }
    }
}
