#include "EmpireLogs/JoinTracker.h"

#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <mutex>

namespace EmpireLogs
{
    JoinTracker::JoinTracker(SharedState& shared_state)
        : m_shared_state(shared_state)
    {
    }

    void JoinTracker::InstallHook()
    {
        m_hook_ids = RC::Unreal::UObjectGlobals::RegisterHook(
            STR("/Script/Pal.PalPlayerCharacter:OnCompleteInitializeParameter"),
            &JoinTracker::OnHook,
            nullptr,
            this);

        PCL_Log("Hook [OnCompleteInitializeParameter] IDs: ({}, {})", m_hook_ids.first, m_hook_ids.second);
    }

    void JoinTracker::OnHook(RC::Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        if (auto* self = static_cast<JoinTracker*>(custom_data))
        {
            self->Handle(context);
        }
    }

    void JoinTracker::Handle(RC::Unreal::UnrealScriptFunctionCallableContext& context)
    {
        auto* player_character = context.Context;
        if (!player_character)
        {
            return;
        }

        auto* player_state = m_shared_state.TryReadObjectProp(player_character, STR("PlayerState"));
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
            m_shared_state.WriteLog();
        }

        PCL_Log("JOIN | {} | guild={} | id={}", RC::ensure_str(player_name), RC::ensure_str(guild_name), RC::ensure_str(guild_id.empty() ? std::string{"?"} : guild_id));
    }
}
