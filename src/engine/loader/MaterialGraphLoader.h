#pragma once

#include "../System.h"
#include "../materialgraph/MaterialGraph.h"

#include <string>

namespace Atlas::Loader {

    class MaterialGraphLoader {

    public:
        // Loads and compiles a graph, compilation errors are logged and don't fail the load
        static Ref<MaterialGraph::Graph> LoadGraph(const std::string& filename);

        static void SaveGraph(const Ref<MaterialGraph::Graph>& graph, const std::string& filename);

    };

}
