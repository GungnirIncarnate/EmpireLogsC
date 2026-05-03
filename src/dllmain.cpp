#include "Mod/CppUserModBase.hpp"
#include "EmpireLogs/MainLoader.h"
#include "Logger/Logger.h"

class EmpireLogsMod : public RC::CppUserModBase
{
public:
    EmpireLogsMod()
        : CppUserModBase()
    {
        ModName = STR("EmpireLogs");
        ModVersion = STR("0.1.0");
        ModDescription = STR("Track PalBox dismantle/build events using UE4SS C++ hooks.");
        ModAuthors = STR("GungnirIncarnate");
    }

    ~EmpireLogsMod() override = default;

    auto on_unreal_init() -> void override
    {
        PCL_Log("EmpireLogs on_unreal_init.");
        m_MainLoader.Initialize();
    }

private:
    EmpireLogs::MainLoader m_MainLoader;
};

#define EMPIRELOGS_API __declspec(dllexport)

extern "C"
{
    EMPIRELOGS_API RC::CppUserModBase* start_mod()
    {
        return new EmpireLogsMod();
    }

    EMPIRELOGS_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
