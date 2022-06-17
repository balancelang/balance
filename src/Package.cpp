#include "headers/Package.h"
#include "headers/Builder.h"
#include "headers/Main.h"

#include <map>
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"

using namespace std;

void Package::load() {
    FILE* fp = fopen(this->packageJsonPath.c_str(), "r");

    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

    this->document.ParseStream(is);

    fclose(fp);
}

void Package::throwIfMissing(std::string property) {
    if (!this->document.HasMember(property.c_str())) {
        throw std::runtime_error("Missing property: " + property);
    }
}

void Package::populate() {
    // Name
    throwIfMissing("name");
    this->name = this->document["name"].GetString();

    // Version
    throwIfMissing("version");
    this->version = this->document["version"].GetString();

    // Entrypoints
    throwIfMissing("entrypoints");
    const rapidjson::Value & entrypoints = document["entrypoints"];
    for (rapidjson::Value::ConstMemberIterator itr = entrypoints.MemberBegin(); itr != entrypoints.MemberEnd(); ++itr)
    {
        std::string entrypointName = itr->name.GetString();
        std::string entrypointValue = itr->value.GetString();
        this->entrypoints[entrypointName] = entrypointValue;

        // TODO: Check if entrypointValue exists?
    }

    // Dependencies
}

bool Package::execute() {
    // Load json file into properties
    this->load();
    this->populate();

    return this->compileAndPersist();
}

bool Package::compileAndPersist() {
    // Walk package and build modules
    std::vector<Module *> modules;

    for (auto const & entryPoint : this->entrypoints) {
        // TODO: For now we just build the first entrypoint
        this->compileRecursively(entryPoint.second, &modules);
        // Persist modules as binary
        writeModuleToFile(entryPoint.first, modules);
        break;
    }

    return true;
}

void Package::compileRecursively(std::string path, std::vector<Module *> * modules) {
    llvm::Module * module = buildModuleFromPath(path);
    modules->push_back(module);
}