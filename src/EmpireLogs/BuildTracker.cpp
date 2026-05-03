#include "EmpireLogs/BuildTracker.h"

#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/NameTypes.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <mutex>
#include <string>

namespace EmpireLogs
{
    namespace
    {
        struct RequestBuildParams
        {
            RC::Unreal::FName BuildObjectName{};
        };
    }

    BuildTracker::BuildTracker(SharedState& shared_state)
        : m_shared_state(shared_state)
    {
    }

    void BuildTracker::InstallHook()
    {
        m_hook_ids = RC::Unreal::UObjectGlobals::RegisterHook(
            STR("/Script/Pal.PalNetworkPlayerComponent:RequestBuild_ToServer"),
            &BuildTracker::OnHook,
            nullptr,
            this);

        PCL_Log("Hook [RequestBuild_ToServer] IDs: ({}, {})", m_hook_ids.first, m_hook_ids.second);
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
        const auto& params = context.GetParams<RequestBuildParams>();
        const auto build_name = RC::to_string(params.BuildObjectName.ToString());
        if (build_name.find("PalBoxV2") == std::string::npos)
        {
            return;
        }

        auto* player_state = m_shared_state.ResolvePlayerStateFromContext(context.Context);
        if (!player_state)
        {
            return;
        }

        const auto [player_name, guild] = m_shared_state.ResolvePlayerNameAndGuild(player_state);
        const auto guild_id = m_shared_state.ResolveGuildId(guild);
        const auto guild_name = m_shared_state.ResolveGuildName(guild);

        {
            std::lock_guard<std::mutex> lock{m_shared_state.GetMutex()};
            m_shared_state.RegisterPlayer(player_name, guild_id, guild_name);

            if (!guild_id.empty())
            {
                const auto new_count = m_shared_state.IncrementCamp(guild_id);
                PCL_Log("BUILD | {} | guild={} | camps={}", RC::ensure_str(player_name), RC::ensure_str(guild_name), new_count);
            }
            else
            {
                PCL_WarnLog("BUILD | {} | guild unknown", RC::ensure_str(player_name));
            }

            m_shared_state.WriteLog();
        }
    }
}
