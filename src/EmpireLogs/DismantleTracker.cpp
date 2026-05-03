#include "EmpireLogs/DismantleTracker.h"

#include "EmpireLogs/SharedState.h"
#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UFunctionStructs.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <cstdint>
#include <mutex>
#include <string>

namespace EmpireLogs
{
    namespace
    {
        struct GuidLike
        {
            uint32_t A{};
            uint32_t B{};
            uint32_t C{};
            uint32_t D{};
        };

        struct RequestDismantleParams
        {
            GuidLike InstanceId{};
        };

        struct FindModelParams
        {
            GuidLike InstanceId{};
            RC::Unreal::UObject* ReturnValue{};
        };

        struct GetConcreteModelParams
        {
            bool bIsForce{};
            RC::Unreal::UObject* ReturnValue{};
        };
    }

    DismantleTracker::DismantleTracker(SharedState& shared_state)
        : m_shared_state(shared_state)
    {
    }

    void DismantleTracker::InstallHook()
    {
        m_hook_ids = RC::Unreal::UObjectGlobals::RegisterHook(
            STR("/Script/Pal.PalNetworkMapObjectComponent:RequestDismantleObject_ToServer"),
            &DismantleTracker::OnHook,
            nullptr,
            this);

        PCL_Log("Hook [RequestDismantleObject_ToServer] IDs: ({}, {})", m_hook_ids.first, m_hook_ids.second);
    }

    void DismantleTracker::OnHook(RC::Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data)
    {
        if (auto* self = static_cast<DismantleTracker*>(custom_data))
        {
            self->Handle(context);
        }
    }

    void DismantleTracker::Handle(RC::Unreal::UnrealScriptFunctionCallableContext& context)
    {
        auto* comp = context.Context;
        if (!comp) { return; }

        auto* outer = comp->GetOuterPrivate();
        if (!outer) { return; }

        const auto& dismantle_params = context.GetParams<RequestDismantleParams>();

        auto* map_object_manager = RC::Unreal::UObjectGlobals::FindFirstOf(STR("PalMapObjectManager"));
        auto* find_model_fn = RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UFunction*>(
            nullptr,
            nullptr,
            STR("/Script/Pal.PalMapObjectManager:FindModel"));

        auto* get_concrete_model_fn = RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UFunction*>(
            nullptr,
            nullptr,
            STR("/Script/Pal.PalMapObjectModel:GetConcreteModel"));

        bool is_pal_box = false;

        if (map_object_manager && find_model_fn && get_concrete_model_fn)
        {
            FindModelParams find_model_params{};
            find_model_params.InstanceId = dismantle_params.InstanceId;
            map_object_manager->ProcessEvent(find_model_fn, &find_model_params);

            if (auto* model = find_model_params.ReturnValue)
            {
                GetConcreteModelParams get_concrete_model_params{};
                get_concrete_model_params.bIsForce = true;
                model->ProcessEvent(get_concrete_model_fn, &get_concrete_model_params);

                if (auto* concrete_model = get_concrete_model_params.ReturnValue)
                {
                    auto* concrete_class = concrete_model->GetClassPrivate();
                    if (concrete_class)
                    {
                        const auto concrete_class_name = RC::to_string(concrete_class->GetNamePrivate().ToString());
                        is_pal_box =
                            concrete_class_name == "BP_MapObjectBaseCampModel_C" ||
                            concrete_class_name == "PalMapObjectBaseCampModel";
                    }
                }
            }
        }

        if (!is_pal_box) { return; }

        auto* owner = m_shared_state.TryReadObjectProp(outer, STR("Owner"));
        if (!owner)
        {
            owner = m_shared_state.TryReadObjectProp(comp, STR("Owner"));
        }
        if (!owner) { return; }

        auto* player_state = m_shared_state.TryReadObjectProp(owner, STR("PlayerState"));
        if (!player_state)
        {
            player_state = m_shared_state.ResolvePlayerStateFromContext(comp);
        }
        if (!player_state) { return; }

        const auto [player_name, guild] = m_shared_state.ResolvePlayerNameAndGuild(player_state);
        const auto guild_id = m_shared_state.ResolveGuildId(guild);
        const auto guild_name = m_shared_state.ResolveGuildName(guild);

        std::lock_guard<std::mutex> lock{m_shared_state.GetMutex()};
        m_shared_state.RegisterPlayer(player_name, guild_id, guild_name);

        if (!guild_id.empty())
        {
            const auto new_count = m_shared_state.DecrementCamp(guild_id);
            PCL_Log("DISMANTLE | {} | guild={} | camps={}", RC::ensure_str(player_name), RC::ensure_str(guild_name), new_count);
        }
        else
        {
            PCL_WarnLog("DISMANTLE | {} | guild unknown", RC::ensure_str(player_name));
        }

        m_shared_state.WriteLog();
    }
}
