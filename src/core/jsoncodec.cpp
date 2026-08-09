#include "jsoncodec.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <random>

namespace core {
namespace {

using json = nlohmann::json;

json comboToJson(const ComboGages& c)
{
    return json{ { "line", c.line }, { "column", c.column }, { "diagonal", c.diagonal } };
}

ComboGages comboFromJson(const json& j)
{
    ComboGages c;
    if (j.contains("line")) c.line = j["line"].get<std::string>();
    if (j.contains("column")) c.column = j["column"].get<std::string>();
    if (j.contains("diagonal")) c.diagonal = j["diagonal"].get<std::string>();
    return c;
}

json multToJson(const Multipliers& m)
{
    return json{ { "line", m.line }, { "column", m.column },
                 { "diagonal", m.diagonal }, { "full", m.full } };
}

Multipliers multFromJson(const json& j)
{
    Multipliers m;
    if (j.contains("line")) m.line = j["line"].get<int>();
    if (j.contains("column")) m.column = j["column"].get<int>();
    if (j.contains("diagonal")) m.diagonal = j["diagonal"].get<int>();
    if (j.contains("full")) m.full = j["full"].get<int>();
    return m;
}

json cellToJson(const GridCell& cell)
{
    return json{ { "label", cell.label }, { "points", cell.points }, { "rate", cell.rate },
                 { "gage", cell.gage }, { "gageHP", cell.gageHP }, { "isFree", cell.isFree } };
}

GridCell cellFromJson(const json& j)
{
    GridCell cell;
    if (j.contains("label")) cell.label = j["label"].get<std::string>();
    if (j.contains("points")) cell.points = j["points"].get<int>();
    if (j.contains("rate")) cell.rate = j["rate"].get<int>();
    if (j.contains("gage")) cell.gage = j["gage"].get<std::string>();
    if (j.contains("gageHP")) cell.gageHP = j["gageHP"].get<int>();
    if (j.contains("isFree")) cell.isFree = j["isFree"].get<bool>();
    return cell;
}

json gridToJson(const PlayerGrid& g)
{
    json cells = json::array();
    for (const auto& row : g.cells) {
        json r = json::array();
        for (const auto& cell : row)
            r.push_back(cellToJson(cell));
        cells.push_back(std::move(r));
    }
    return json{ { "player", g.player }, { "cells", std::move(cells) } };
}

PlayerGrid gridFromJson(const json& j)
{
    PlayerGrid g;
    if (j.contains("player")) g.player = j["player"].get<std::string>();
    if (j.contains("cells") && j["cells"].is_array()) {
        for (const auto& row : j["cells"]) {
            std::vector<GridCell> r;
            if (row.is_array()) {
                for (const auto& c : row)
                    r.push_back(cellFromJson(c));
            }
            g.cells.push_back(std::move(r));
        }
    }
    return g;
}

json projectToJsonObj(const Project& p)
{
    json players = json::array();
    for (const auto& pl : p.players)
        players.push_back(json{ { "name", pl.name } });

    json cases = json::array();
    for (const auto& c : p.cases)
        cases.push_back(json{ { "label", c.label }, { "points", c.points }, { "rate", c.rate } });

    json gages = json::array();
    for (const auto& g : p.gages)
        gages.push_back(json{
            { "description", g.description },
            { "hp", g.hp },
            { "number", g.number },
            { "rate", g.rate },
        });

    json grids = json::array();
    for (const auto& gr : p.grids)
        grids.push_back(gridToJson(gr));

    return json{
        { "id", p.id },
        { "title", p.title },
        { "description", p.description },
        { "createdAt", p.createdAt },
        { "updatedAt", p.updatedAt },
        { "gridSize", p.gridSize },
        { "players", std::move(players) },
        { "startHP", p.startHP },
        { "freeCenter", p.freeCenter },
        { "gageMode", p.gageMode },
        { "comboGages", comboToJson(p.comboGages) },
        { "multipliers", multToJson(p.multipliers) },
        { "cases", std::move(cases) },
        { "gages", std::move(gages) },
        { "grids", std::move(grids) },
    };
}

Project projectFromJsonObj(const json& j)
{
    Project p = JsonCodec::defaultProject();
    if (j.contains("id")) p.id = j["id"].get<std::string>();
    if (j.contains("title")) p.title = j["title"].get<std::string>();
    if (j.contains("description")) p.description = j["description"].get<std::string>();
    if (j.contains("createdAt")) p.createdAt = j["createdAt"].get<int64_t>();
    if (j.contains("updatedAt")) p.updatedAt = j["updatedAt"].get<int64_t>();
    if (j.contains("gridSize")) p.gridSize = j["gridSize"].get<int>();
    if (j.contains("startHP")) p.startHP = j["startHP"].get<int>();
    if (j.contains("freeCenter")) p.freeCenter = j["freeCenter"].get<bool>();
    if (j.contains("gageMode")) p.gageMode = j["gageMode"].get<bool>();
    if (j.contains("comboGages")) p.comboGages = comboFromJson(j["comboGages"]);
    if (j.contains("multipliers")) p.multipliers = multFromJson(j["multipliers"]);

    if (j.contains("players") && j["players"].is_array()) {
        p.players.clear();
        for (const auto& pl : j["players"]) {
            Player player;
            if (pl.contains("name")) player.name = pl["name"].get<std::string>();
            p.players.push_back(std::move(player));
        }
    }

    if (j.contains("cases") && j["cases"].is_array()) {
        p.cases.clear();
        for (const auto& c : j["cases"]) {
            Case cs;
            if (c.contains("label")) cs.label = c["label"].get<std::string>();
            if (c.contains("points")) cs.points = c["points"].get<int>();
            if (c.contains("rate")) cs.rate = c["rate"].get<int>();
            p.cases.push_back(std::move(cs));
        }
    }

    if (j.contains("gages") && j["gages"].is_array()) {
        p.gages.clear();
        int idx = 0;
        for (const auto& g : j["gages"]) {
            Gage ga;
            if (g.contains("description")) ga.description = g["description"].get<std::string>();
            if (g.contains("hp")) ga.hp = g["hp"].get<int>();
            // Anciens exports : number = rang 1-based, rate = 100.
            if (g.contains("number")) ga.number = g["number"].get<int>();
            else ga.number = idx + 1;
            if (g.contains("rate")) ga.rate = g["rate"].get<int>();
            else ga.rate = 100;
            if (ga.number < 1) ga.number = 1;
            if (ga.rate < 0) ga.rate = 0;
            p.gages.push_back(std::move(ga));
            ++idx;
        }
    }

    if (j.contains("grids") && j["grids"].is_array()) {
        p.grids.clear();
        for (const auto& gr : j["grids"])
            p.grids.push_back(gridFromJson(gr));
    }

    if (p.id.empty())
        p.id = JsonCodec::makeId();
    return p;
}

} // namespace

std::string JsonCodec::makeId()
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, 0xFFFFF);
    return "proj_" + std::to_string(now) + "_" + std::to_string(dist(rng));
}

Project JsonCodec::defaultProject()
{
    Project p;
        p.id = JsonCodec::makeId();
    p.title = "Nouveau Bingo";
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    p.createdAt = now;
    p.updatedAt = now;
    p.players = { { "Joueur 1" }, { "Joueur 2" } };
    return p;
}

std::string JsonCodec::projectToJson(const Project& p, bool pretty)
{
    const auto j = projectToJsonObj(p);
    return pretty ? j.dump(2) : j.dump();
}

Project JsonCodec::projectFromJson(const std::string& jsonStr, bool* ok)
{
    try {
        const auto j = json::parse(jsonStr);
        if (ok) *ok = true;
        // Enveloppe sync/export : { v, project, playChecks }.
        if (j.is_object() && j.contains("project") && j["project"].is_object())
            return projectFromJsonObj(j["project"]);
        return projectFromJsonObj(j);
    } catch (...) {
        if (ok) *ok = false;
        return {};
    }
}

namespace {

json playChecksToJson(const std::map<std::string, std::string>& playChecks)
{
    json obj = json::object();
    for (const auto& [player, checksJson] : playChecks) {
        try {
            obj[player] = json::parse(checksJson);
        } catch (...) {
            obj[player] = json::array();
        }
    }
    return obj;
}

std::map<std::string, std::string> playChecksFromJson(const json& j)
{
    std::map<std::string, std::string> out;
    if (!j.is_object())
        return out;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_array())
            out.emplace(it.key(), it.value().dump());
        else if (it.value().is_string())
            out.emplace(it.key(), it.value().get<std::string>());
    }
    return out;
}

ProjectBundle bundleFromJsonObj(const json& j)
{
    ProjectBundle b;
    if (j.contains("project") && j["project"].is_object()) {
        b.project = projectFromJsonObj(j["project"]);
        if (j.contains("playChecks")) {
            b.hasPlayChecks = true;
            b.playChecks = playChecksFromJson(j["playChecks"]);
        }
        return b;
    }
    // Projet nu ; playChecks optionnel au sommet (ignoré par projectFromJsonObj).
    b.project = projectFromJsonObj(j);
    if (j.contains("playChecks")) {
        b.hasPlayChecks = true;
        b.playChecks = playChecksFromJson(j["playChecks"]);
    }
    return b;
}

} // namespace

std::string JsonCodec::projectBundleToJson(const ProjectBundle& bundle, bool pretty)
{
    json env = json{
        { "v", 1 },
        { "project", projectToJsonObj(bundle.project) },
        { "playChecks", playChecksToJson(bundle.playChecks) },
    };
    return pretty ? env.dump(2) : env.dump();
}

ProjectBundle JsonCodec::projectBundleFromJson(const std::string& jsonStr, bool* ok)
{
    try {
        const auto j = json::parse(jsonStr);
        if (ok) *ok = true;
        return bundleFromJsonObj(j);
    } catch (...) {
        if (ok) *ok = false;
        return {};
    }
}

std::string JsonCodec::exportAll(const std::vector<Project>& projects, bool pretty)
{
    json arr = json::array();
    for (const auto& p : projects)
        arr.push_back(projectToJsonObj(p));
    json wrapper{ { "version", 1 }, { "projects", std::move(arr) } };
    return pretty ? wrapper.dump(2) : wrapper.dump();
}

std::string JsonCodec::exportAllBundles(const std::vector<ProjectBundle>& bundles, bool pretty)
{
    json arr = json::array();
    for (const auto& b : bundles) {
        arr.push_back(json{
            { "v", 1 },
            { "project", projectToJsonObj(b.project) },
            { "playChecks", playChecksToJson(b.playChecks) },
        });
    }
    json wrapper{ { "version", 1 }, { "projects", std::move(arr) } };
    return pretty ? wrapper.dump(2) : wrapper.dump();
}

std::vector<Project> JsonCodec::importAll(const std::string& jsonStr, bool* ok)
{
    std::vector<Project> out;
    for (const auto& b : importAllBundles(jsonStr, ok))
        out.push_back(b.project);
    return out;
}

std::vector<ProjectBundle> JsonCodec::importAllBundles(const std::string& jsonStr, bool* ok)
{
    std::vector<ProjectBundle> out;
    try {
        const json data = json::parse(jsonStr);
        json arr;
        if (data.contains("version") && data["version"] == 1 && data.contains("projects"))
            arr = data["projects"];
        else if (data.is_array())
            arr = data;
        else
            arr = json::array({ data });

        for (const auto& item : arr) {
            if (!item.is_object())
                continue;
            out.push_back(bundleFromJsonObj(item));
        }
        if (ok) *ok = true;
    } catch (...) {
        if (ok) *ok = false;
    }
    return out;
}

} // namespace core
