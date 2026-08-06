#pragma once

#include "bingotypes.h"

#include <optional>
#include <string>
#include <vector>

namespace core {

class JsonCodec
{
public:
    static std::string projectToJson(const Project& p, bool pretty = true);
    static Project     projectFromJson(const std::string& json, bool* ok = nullptr);

    static std::string exportAll(const std::vector<Project>& projects, bool pretty = true);
    static std::vector<Project> importAll(const std::string& json, bool* ok = nullptr);

    static std::string makeId();
    static Project     defaultProject();
};

} // namespace core
