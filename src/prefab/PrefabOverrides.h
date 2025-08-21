#pragma once
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

struct OverrideOp {
    std::string Op;   // set, insert, erase, addComponent, removeComponent, addEntity, removeEntity, reparent
    std::string Path; // path grammar per spec
    nlohmann::json Value; // for set/insert; component/entity objects
};

struct PrefabOverrides {
    std::vector<OverrideOp> Ops;
};


