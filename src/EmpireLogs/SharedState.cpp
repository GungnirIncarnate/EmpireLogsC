#include "EmpireLogs/SharedState.h"

#include "Logger/Logger.h"

#include <Helpers/String.hpp>

#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>

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

        auto join_strings(const std::vector<std::string>& values, const std::string_view separator) -> std::string
        {
            if (values.empty())
            {
                return {};
            }

            std::ostringstream out;
            for (size_t index = 0; index < values.size(); ++index)
            {
                if (index > 0)
                {
                    out << separator;
                }
                out << values[index];
            }
            return out.str();
        }
    }

    void SharedState::SetupPaths(const std::filesystem::path& working_dir)
    {
        m_log_path = working_dir / "Mods" / "EmpireLogsC" / "EmpireLog.json";
        m_guild_export_path = working_dir / "PalDefender" / "guildexport.json";
    }

    auto SharedState::TryReadObjectProp(RC::Unreal::UObject* object, const wchar_t* prop_name) -> RC::Unreal::UObject*
    {
        if (!object)
        {
            return nullptr;
        }

        if (auto** value = object->GetValuePtrByPropertyNameInChain<RC::Unreal::UObject*>(prop_name))
        {
            return *value;
        }

        return nullptr;
    }

    auto SharedState::TryReadStringProp(RC::Unreal::UObject* object, const wchar_t* prop_name) -> std::string
    {
        if (!object)
        {
            return {};
        }

        if (auto* value = object->GetValuePtrByPropertyNameInChain<RC::Unreal::FString>(prop_name))
        {
            return RC::to_string(**value);
        }

        if (auto* value = object->GetValuePtrByPropertyNameInChain<RC::Unreal::FName>(prop_name))
        {
            return RC::to_string(value->ToString());
        }

        return {};
    }

    auto SharedState::ResolvePlayerStateFromContext(RC::Unreal::UObject* context_object) -> RC::Unreal::UObject*
    {
        if (!context_object)
        {
            return nullptr;
        }

        auto* outer = context_object->GetOuterPrivate();
        if (!outer)
        {
            return nullptr;
        }

        auto* owner = TryReadObjectProp(outer, STR("Owner"));
        if (!owner)
        {
            owner = TryReadObjectProp(context_object, STR("Owner"));
        }
        if (!owner)
        {
            return nullptr;
        }

        return TryReadObjectProp(owner, STR("PlayerState"));
    }

    auto SharedState::ResolvePlayerNameAndGuild(RC::Unreal::UObject* player_state) -> std::pair<std::string, RC::Unreal::UObject*>
    {
        std::string player_name = TryReadStringProp(player_state, STR("PlayerNamePrivate"));
        if (player_name.empty())
        {
            player_name = TryReadStringProp(player_state, STR("PlayerName"));
        }
        if (player_name.empty())
        {
            player_name = "Unknown";
        }

        auto* guild = TryReadObjectProp(player_state, STR("GuildBelongTo"));
        return {player_name, guild};
    }

    auto SharedState::ResolveGuildId(RC::Unreal::UObject* guild) -> std::string
    {
        if (!guild)
        {
            return {};
        }

        const auto* guid = guild->GetValuePtrByPropertyNameInChain<GuidLike>(STR("Id"));
        if (!guid)
        {
            return {};
        }

        if (guid->A == 0 && guid->B == 0 && guid->C == 0 && guid->D == 0)
        {
            return {};
        }

        char buffer[48]{};
        std::snprintf(buffer, sizeof(buffer), "%08X-%08X-%08X-%08X", guid->A, guid->B, guid->C, guid->D);
        return buffer;
    }

    auto SharedState::ResolveGuildName(RC::Unreal::UObject* guild) -> std::string
    {
        if (!guild)
        {
            return "Unknown Guild";
        }

        std::string guild_name = TryReadStringProp(guild, STR("GuildName"));
        if (guild_name.empty())
        {
            guild_name = TryReadStringProp(guild, STR("Name"));
        }
        if (guild_name.empty())
        {
            guild_name = "Unknown Guild";
        }

        return guild_name;
    }

    void SharedState::EnsureGuild(const std::string& guild_id, const std::string& guild_name)
    {
        if (guild_id.empty())
        {
            return;
        }

        auto& guild = m_guilds[guild_id];
        if (!guild_name.empty())
        {
            guild.name = guild_name;
        }
    }

    void SharedState::RegisterPlayer(const std::string& player_name, const std::string& guild_id, const std::string& guild_name)
    {
        if (player_name.empty())
        {
            return;
        }

        EnsureGuild(guild_id, guild_name);

        const auto existing = m_players.find(player_name);
        if (existing != m_players.end() && !existing->second.guild_id.empty() && existing->second.guild_id != guild_id)
        {
            const auto old_it = m_guilds.find(existing->second.guild_id);
            if (old_it != m_guilds.end())
            {
                auto& members = old_it->second.member_names;
                members.erase(std::remove(members.begin(), members.end(), player_name), members.end());
            }
        }

        m_players[player_name] = PlayerInfo{guild_id, guild_name};

        if (!guild_id.empty())
        {
            auto& members = m_guilds[guild_id].member_names;
            if (std::find(members.begin(), members.end(), player_name) == members.end())
            {
                members.emplace_back(player_name);
            }
        }
    }

    int SharedState::IncrementCamp(const std::string& guild_id)
    {
        if (guild_id.empty())
        {
            return 0;
        }

        auto& guild = m_guilds[guild_id];
        guild.camp_count = std::max(0, guild.camp_count) + 1;
        return guild.camp_count;
    }

    int SharedState::DecrementCamp(const std::string& guild_id)
    {
        const auto guild_it = m_guilds.find(guild_id);
        if (guild_it == m_guilds.end())
        {
            return 0;
        }

        guild_it->second.camp_count = std::max(0, guild_it->second.camp_count - 1);
        return guild_it->second.camp_count;
    }

    auto SharedState::EscapeJson(std::string value) const -> std::string
    {
        size_t pos = 0;
        while ((pos = value.find('\\', pos)) != std::string::npos)
        {
            value.replace(pos, 1, "\\\\");
            pos += 2;
        }

        pos = 0;
        while ((pos = value.find('"', pos)) != std::string::npos)
        {
            value.replace(pos, 1, "\\\"");
            pos += 2;
        }

        return value;
    }

    void SharedState::WriteLog()
    {
        std::error_code ec;
        std::filesystem::create_directories(m_log_path.parent_path(), ec);

        std::ofstream out{m_log_path, std::ios::binary | std::ios::trunc};
        if (!out.is_open())
        {
            PCL_ErrorLog("Failed to open {} for write", RC::ensure_str(m_log_path.string()));
            return;
        }

        out << "{\n";
        out << "  \"guilds\": {\n";

        bool first_guild = true;
        for (const auto& [guild_id, guild] : m_guilds)
        {
            if (!first_guild)
            {
                out << ",\n";
            }
            first_guild = false;

            std::vector<std::string> member_literals{};
            member_literals.reserve(guild.member_names.size());
            for (const auto& member : guild.member_names)
            {
                member_literals.emplace_back("\"" + EscapeJson(member) + "\"");
            }

            out << "    \"" << EscapeJson(guild_id) << "\": {\"name\":\"" << EscapeJson(guild.name)
                << "\",\"camp_count\":" << guild.camp_count
                << ",\"members\":[" << join_strings(member_literals, ",") << "]}";
        }

        out << "\n  },\n";
        out << "  \"players\": {\n";

        bool first_player = true;
        for (const auto& [player_name, player] : m_players)
        {
            if (!first_player)
            {
                out << ",\n";
            }
            first_player = false;

            out << "    \"" << EscapeJson(player_name) << "\": {\"guild_id\":\"" << EscapeJson(player.guild_id)
                << "\",\"guild_name\":\"" << EscapeJson(player.guild_name) << "\"}";
        }

        out << "\n  }\n";
        out << "}\n";
    }

    void SharedState::LoadLog()
    {
        std::ifstream in{m_log_path, std::ios::binary};
        if (!in.is_open())
        {
            PCL_Log("No existing EmpireLog.json - starting fresh.");
            return;
        }

        const std::string raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

        std::smatch guilds_match{};
        const std::regex guilds_section_regex{"\"guilds\"\\s*:\\s*\\{([\\s\\S]*?)\\}\\s*,\\s*\"players\""};
        if (std::regex_search(raw, guilds_match, guilds_section_regex) && guilds_match.size() > 1)
        {
            const std::string section = guilds_match[1].str();
            const std::regex guild_regex{"\"([A-Fa-f0-9\\-]+)\"\\s*:\\s*\\{\\s*\"name\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"camp_count\"\\s*:\\s*(\\d+)\\s*,\\s*\"members\"\\s*:\\s*\\[([^\\]]*)\\]\\s*\\}"};

            for (std::sregex_iterator it{section.begin(), section.end(), guild_regex}; it != std::sregex_iterator{}; ++it)
            {
                const auto guild_id = (*it)[1].str();
                GuildInfo guild{};
                guild.name = (*it)[2].str();
                guild.camp_count = std::stoi((*it)[3].str());

                const std::string members = (*it)[4].str();
                const std::regex member_regex{"\"([^\"]+)\""};
                for (std::sregex_iterator mit{members.begin(), members.end(), member_regex}; mit != std::sregex_iterator{}; ++mit)
                {
                    guild.member_names.emplace_back((*mit)[1].str());
                }

                m_guilds[guild_id] = std::move(guild);
            }
        }

        std::smatch players_match{};
        const std::regex players_section_regex{"\"players\"\\s*:\\s*\\{([\\s\\S]*?)\\}\\s*\\}"};
        if (std::regex_search(raw, players_match, players_section_regex) && players_match.size() > 1)
        {
            const std::string section = players_match[1].str();
            const std::regex player_regex{"\"([^\"]+)\"\\s*:\\s*\\{\\s*\"guild_id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"guild_name\"\\s*:\\s*\"([^\"]*)\"\\s*\\}"};

            for (std::sregex_iterator it{section.begin(), section.end(), player_regex}; it != std::sregex_iterator{}; ++it)
            {
                m_players[(*it)[1].str()] = PlayerInfo{(*it)[2].str(), (*it)[3].str()};
            }
        }

        PCL_Log("Restored: {} guild(s), {} player(s)", m_guilds.size(), m_players.size());
    }

    void SharedState::SeedFromGuildExport()
    {
        std::ifstream in{m_guild_export_path, std::ios::binary};
        if (!in.is_open())
        {
            PCL_Log("No guildexport.json found - member counts from EmpireLog only.");
            return;
        }

        const std::string raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        int seeded = 0;

        const std::regex guild_regex{"\"([A-Fa-f0-9\\-]+)\"\\s*:\\s*\\{([\\s\\S]*?)\\}\"?\\s*(,|$)"};
        for (std::sregex_iterator it{raw.begin(), raw.end(), guild_regex}; it != std::sregex_iterator{}; ++it)
        {
            const auto guild_id = (*it)[1].str();
            const auto section = (*it)[2].str();

            std::smatch name_match{};
            std::string guild_name{"Unknown Guild"};
            if (std::regex_search(section, name_match, std::regex{"\"Name\"\\s*:\\s*\"([^\"]*)\""}) && name_match.size() > 1)
            {
                guild_name = name_match[1].str();
            }

            EnsureGuild(guild_id, guild_name);

            std::smatch camp_match{};
            if (std::regex_search(section, camp_match, std::regex{"\"CampNum\"\\s*:\\s*(\\d+)"}) && camp_match.size() > 1)
            {
                m_guilds[guild_id].camp_count = std::max(m_guilds[guild_id].camp_count, std::stoi(camp_match[1].str()));
            }

            const std::regex member_regex{"\"NickName\"\\s*:\\s*\"([^\"]+)\""};
            for (std::sregex_iterator mit{section.begin(), section.end(), member_regex}; mit != std::sregex_iterator{}; ++mit)
            {
                const auto nickname = (*mit)[1].str();
                if (nickname.empty())
                {
                    continue;
                }

                auto& members = m_guilds[guild_id].member_names;
                if (std::find(members.begin(), members.end(), nickname) == members.end())
                {
                    members.emplace_back(nickname);
                    ++seeded;
                }

                if (!m_players.contains(nickname))
                {
                    m_players[nickname] = PlayerInfo{guild_id, guild_name};
                }
            }
        }

        PCL_Log("Guild export seeded: {} member(s) added", seeded);
        if (seeded > 0)
        {
            WriteLog();
        }
    }
}
