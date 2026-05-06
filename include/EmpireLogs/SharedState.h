#pragma once

#include "EmpireLogs/Types.h"

#include <String/StringType.hpp>

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RC::Unreal
{
    class UObject;
}

namespace EmpireLogs
{
    class SharedState
    {
    public:
        void SetupPaths(const std::filesystem::path& working_dir);
        void LoadLog();
        void SeedFromGuildExport();
        void WriteLog();

        void EnsureGuild(const std::string& guild_id, const std::string& guild_name);
        void RegisterPlayer(const std::string& player_name, const std::string& guild_id, const std::string& guild_name);
        int QueryLiveCampCount(RC::Unreal::UObject* guild);
        auto QueryLiveMembers(RC::Unreal::UObject* guild) -> std::vector<std::string>;
        void SetCampCount(const std::string& guild_id, int count);
        int GetCampCount(const std::string& guild_id) const;
        void SyncGuildMembers(const std::string& guild_id, const std::vector<std::string>& members);
        int IncrementCamp(const std::string& guild_id);
        int DecrementCamp(const std::string& guild_id);

        auto TryReadObjectProp(RC::Unreal::UObject* object, const wchar_t* prop_name) -> RC::Unreal::UObject*;
        auto TryReadStringProp(RC::Unreal::UObject* object, const wchar_t* prop_name) -> std::string;
        auto ResolvePlayerStateFromContext(RC::Unreal::UObject* context_object) -> RC::Unreal::UObject*;
        auto ResolvePlayerNameAndGuild(RC::Unreal::UObject* player_state) -> std::pair<std::string, RC::Unreal::UObject*>;
        auto ResolveGuildId(RC::Unreal::UObject* guild) -> std::string;
        auto ResolveGuildName(RC::Unreal::UObject* guild) -> std::string;
        auto EscapeJson(std::string value) const -> std::string;

        auto GetLogPath() const -> const std::filesystem::path& { return m_log_path; }
        auto GetMutex() -> std::mutex& { return m_state_mutex; }

    private:
        std::filesystem::path m_log_path{};
        std::filesystem::path m_guild_export_path{};
        std::unordered_map<std::string, GuildInfo> m_guilds{};
        std::unordered_map<std::string, PlayerInfo> m_players{};
        std::mutex m_state_mutex{};
    };
}
