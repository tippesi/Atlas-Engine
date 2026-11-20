#pragma once

#include "Common.h"

#include "materialgraph/MaterialGraph.h"

#include <map>
#include <set>

namespace Atlas::Capture {

    // A graph file as the operations of a job edit it. Textures aren't loaded, the nodes keep their paths
    struct GraphDocument {

        Ref<MaterialGraph::Graph> graph;
        std::map<uint32_t, std::string> texturePaths;

    };

    // The graphs of the operations of a job by their asset path. Nothing is written before the checks of the edit passed
    struct MaterialGraphEdits {

        std::map<std::string, GraphDocument> documents;
        // Documents to write, the files still have the state from before the job
        std::set<std::string> changed;
        // Graphs earlier assignMaterialGraph operations put on materials, empty if they removed it (only check uses it)
        std::map<std::string, std::string> assigned;

    };

    // The materialGraph edit operation: creates or changes a graph document and compiles it, compile errors fail it
    json EditMaterialGraph(const json& operation, MaterialGraphEdits& edits);

    // Writes the changed graphs, a .backup of an existing file is kept
    json SaveMaterialGraphs(MaterialGraphEdits& edits);

    // The graph of a document as the engine uses it, with its textures
    Ref<MaterialGraph::Graph> CreateGraph(const GraphDocument& document);

    // Material files of a path or a list of paths, wildcards are allowed in the file names. Fails if nothing matches
    std::vector<std::string> FindMaterialFiles(const json& patterns);

    // The keys of an assignMaterialGraph operation and values without a graph
    void CheckAssignMaterialGraph(const json& operation);

    // Validates the values, textures and resets of an assignMaterialGraph operation against the parameters of a graph
    void CheckGraphValues(const json& operation, const MaterialGraph::CompiledGraph& compiled, const std::string& graphPath);

    // The value of a parameter from a number (all components) or an array (the first components, the others are kept)
    vec4 ToParameterValue(const MaterialGraph::Parameter& parameter, const json& value, vec4 current);

    // The components of the parameter type, rounded for reports
    json ToParameterJson(const MaterialGraph::Parameter& parameter, vec4 value);

    const MaterialGraph::Parameter* FindParameter(const MaterialGraph::CompiledGraph& compiled, const std::string& name);

    // Applies the materialGraph and assignMaterialGraph operations of a job without the engine, issues start with ERROR
    void CheckMaterialGraphOperations(const json& operations, std::vector<std::string>& issues);

    // inspect of a .aematgraph file: nodes, links, parameters and the result of compiling it
    int32_t InspectMaterialGraph(const fs::path& file, const std::vector<std::string>& filters, bool printCode, const fs::path& output);

    // inspect nodes: the node library with the pins of every node
    int32_t InspectMaterialGraphNodes(const std::vector<std::string>& filters);

}
