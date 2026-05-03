#pragma once

#include <string>
#include <vector>

namespace EmpireLogs
{
    struct PlayerInfo
    {
        std::string guild_id{};
        std::string guild_name{};
    };

    struct GuildInfo
    {
        std::string name{"Unknown Guild"};
        int camp_count{0};
        std::vector<std::string> member_names{};
    };
}
