#pragma once

#include <utility>

namespace RC::Unreal
{
    class UnrealScriptFunctionCallableContext;
}

namespace EmpireLogs
{
    class SharedState;

    class BuildTracker
    {
    public:
        explicit BuildTracker(SharedState& shared_state);
        void InstallHook();

    private:
        static void OnHook(RC::Unreal::UnrealScriptFunctionCallableContext& context, void* custom_data);
        void Handle(RC::Unreal::UnrealScriptFunctionCallableContext& context);

    private:
        SharedState& m_shared_state;
        std::pair<int, int> m_hook_ids{};
    };
}
