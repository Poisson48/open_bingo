#pragma once

#include "bingotypes.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace core {

struct ProjectBundle {
    Project project;
    // joueur → JSON array des coches (ex. "[[true,false],…]").
    std::map<std::string, std::string> playChecks;
    // true si le JSON contenait explicitement "playChecks" (même vide).
    bool hasPlayChecks = false;
    // Overlays gage/combo du dernier cochage (JSON array) — source de vérité
    // partagée pour que tous les appareils affichent les mêmes noms.
    std::string playOverlaysJson;
    bool hasPlayOverlays = false;
};

class JsonCodec
{
public:
    static std::string projectToJson(const Project& p, bool pretty = true);
    static Project     projectFromJson(const std::string& json, bool* ok = nullptr);

    // Enveloppe { "v":1, "project":{…}, "playChecks":{…} } — aussi acceptée à l'import
    // sous forme de projet nu (rétrocompat) avec playChecks optionnel au sommet.
    static std::string projectBundleToJson(const ProjectBundle& bundle, bool pretty = true);
    static ProjectBundle projectBundleFromJson(const std::string& json, bool* ok = nullptr);

    static std::string exportAll(const std::vector<Project>& projects, bool pretty = true);
    static std::string exportAllBundles(const std::vector<ProjectBundle>& bundles,
                                        bool pretty = true);
    static std::vector<Project> importAll(const std::string& json, bool* ok = nullptr);
    static std::vector<ProjectBundle> importAllBundles(const std::string& json,
                                                       bool* ok = nullptr);

    static std::string makeId();
    static Project     defaultProject();
};

} // namespace core
