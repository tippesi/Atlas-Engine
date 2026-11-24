# Material graphs

A material graph describes how a material moves vertices and what its surface looks like with connected nodes. Shaders are
generated from it, each graph structure gets its own pipelines. Regular materials aren't affected: their shaders and pipelines
stay exactly the same.

## In the editor

Open a material from the content browser (double click or the context menu). The material editor shows a preview, the properties
and the graph below them. `Create material graph` adds a new graph next to the material, or drop an existing `.aematgraph` on the
slot to share it. Materials are saved together with their graph (Ctrl+S).

A graph has two outputs, and which of them are connected decides what the graph does:

| Connected | Result |
| --- | --- |
| Vertex output | Vertices move, the fragments come from the regular material properties and maps |
| Surface output | The graph describes the surface, the vertices stay. Shadows use the regular pipeline unless `Opacity` is connected |
| Both | Both of the above. `Vertex to surface` nodes hand values from the vertices to the surface |

- Add nodes with `Space`, a right click or by dropping a link on the canvas. `Arrange` lays out all nodes, `F` shows them all.
- Nodes show the stages they end up in with `V` and `F`, nodes which don't contribute are faded.
- Set `Largest displacement` for graphs moving vertices, culling grows the bounds by it. Otherwise moved parts disappear at the
  edges of the screen.
- `Generated code` shows the shaders of the graph.

**Parameters and constants:** parameter nodes appear in the properties of the material. Each material has its own values, and
changing them doesn't compile anything, so materials with the same graph share their pipelines. Constants and values typed
into a node are baked into the shaders. Prefer parameters for what should differ per material. There is no limit on the number of
textures, they are bindless.

**Code:** a `Code` node runs GLSL, e.g. a formula that would take many nodes. Its code is the body of a function: the inputs
added in the inspector are its arguments (name and type), it returns the result type. It can call the functions every shader
has (`common.hsh`), but doesn't see the input of the stage; connect `Position`, `Time` and similar nodes to its inputs instead.
Nodes with the same code share one function. Errors in the code show up when the pipeline compiles, not in the graph.
Graph files and bridge jobs store the inputs as `[["name", "vec3"], ...]` and the result type as `"codeOutputType": "vec3"`,
types are `float`, `vec2`, `vec3` and `vec4`.

**Material type and volume:** the blend mode (opaque, alpha, additive, multiply) and two sidedness are settings of the graph
(`Material type` in the graph panel), they are pipeline state of every material using it. The surface output also has
`Reflectance`, `Transmission` (the amount of the base color passing thin surfaces from behind), `Refractive index`,
`Thickness`, `Attenuation color` and `Attenuation distance`, so materials with a graph describing the surface don't use any
of their regular properties. Graphs saved before these existed take them once from the first material which loads them
(`Material::MigrateGraphSettings`), save the graph to keep them. Values which should differ between materials of one graph
belong into parameters.

**Opacity:** an `Opacity` that varies over the surface cuts out fragments, which turns off early depth tests and costs time even
where nothing is cut out. An `Opacity` that only depends on parameters and constants, e.g. a fade or visibility switch per
material, is evaluated per vertex instead and hides the whole draw, the fragments stay as fast as opaque ones.

## Limits

- The lighting model is the regular one, graphs write into the G-buffer. Materials with a blend mode use the same surface in
  the translucent pass, their `Opacity` is the coverage instead of cutting out fragments (a uniform one still hides the draw at 0).
- Ray tracing pipelines run the surface of graphs in hit groups (devices with shader execution reordering), the hit gets its
  values packed into eight words (`raytracer/graphHit.hsh`). Ray queries on other devices don't run graphs and use the bake
  (see below) and the values of the surface output.
- Impostors show the surface of a graph at the `Time` of its bake settings as it looked when they were generated, without
  displacement and emission. Graphs whose shader doesn't compile show the regular properties, the log says which ones.
  Vegetation clutter and terrain don't use graphs.
- Pipelines compile in the background, meanwhile the regular properties of the material are drawn.

## From code and scripts

```lua
material:SetGraphParameter("Glow", 12.0)
```

```cpp
auto graph = MaterialGraph::Graph::CreateDefault();
auto& noise = graph->AddNode("Simplex noise");
graph->Connect(noise.id, "Result", graph->nodes[1].id, MaterialGraph::NodeLibrary::roughnessPin);
graph->Compile();
material->graph = ResourceHandle<MaterialGraph::Graph>(graph);
```

New nodes are entries in `materialgraph/NodeLibrary.cpp` with a GLSL expression, helpers go into
`data/shader/materialGraph/common.hsh`.

Bridge jobs build and change graphs with the edit operation `materialGraph` and put them on materials with
`assignMaterialGraph`, `check` finds wrong node types, pins and compile errors without loading anything
(`src/bridge/README.md`). `AtlasBridge inspect nodes` lists the node library with its pins, `inspect <graph>`
the nodes of a graph with their links and stages.

## Sharing a graph

A graph is its own resource, any number of materials can reference it. Pipelines are shared, values and textures of parameters
belong to each material. When other materials use the graph, the editor shows them above the canvas, since changing nodes changes
all of them. `Make a copy for this material` gives the material its own graph file.

## Subgraphs

A subgraph (`.aematsubgraph`) is a graph other graphs use as a node, any number of times and also in other subgraphs. Its pins
are `Subgraph input` and `Subgraph output` nodes with a name, a type and an order, it has no outputs of a material. The compiler
puts the nodes of the subgraph in place of the `Subgraph` node, so it costs the same as copies of these nodes: nothing at
runtime is different, and two uses calculate everything twice.

- `Create subgraph` in the context menu of selected nodes (Ctrl+Shift+G) moves them into a new file next to the graph, `Dissolve
  subgraph` puts them back. The content browser creates empty ones. Add existing ones from the node menu or drop them on the canvas,
  a double click on the node opens the subgraph. Changes reach all loaded graphs which use it, the other ones when they load.
- **Inputs** which aren't connected take the value typed into the node, or what is connected to `Default` in the subgraph (the node
  has no value of its own then), e.g. `Texture coordinates`. Renaming a pin keeps the links, also in graphs which aren't loaded.
- **Textures** per use: give the subgraph an input of the type `Texture` and connect a `Texture parameter` of the graph to it. Each
  use can get another one, and each material selects its own. Connect a `Texture parameter` to the `Default` of the input to have one
  without a connection.
- **Parameters** in a subgraph appear in the materials. The same name is the same parameter, also in several uses: what should
  differ per use has to be an input. Static switches in a subgraph are the same for all uses.
- The editor of a subgraph shows outputs named like a pin of a material (`Base color`, `Roughness`, ...) on the preview, otherwise the
  first output as the base color.
- Errors in a subgraph show up at the node using it, with the names of the subgraphs down to the error.

## Parameters

Parameter nodes have a range (materials get a slider), a group (a heading in the material) and an order, all in the inspector next
to the canvas. Without an order colors come first, then values, then textures. Texture parameters also have a default texture, which
materials use until they select their own.

## Baking for ray tracing

Reflections, global illumination, ray traced shadows and the path tracer don't run graphs, they use the properties of the material.
`Bake for ray tracing` in the material window (or the bridge operation `bakeMaterial`) writes the surface of the graph into them:

- **Textures**: maps at one point in time for the positions and texture coordinates of one mesh, in `baked/` next to the material.
  Properties that don't vary only get a value. Texture coordinates have to be from 0 to 1 and shouldn't overlap.
- **Average**: one value per property, which fits every mesh and loses all details.

View dependent nodes see the surface straight on, displacement isn't part of the result. The window says when a bake is outdated,
the material has to be saved to keep it. A bake replaces the maps and values of the material, `Remove bake` brings back the ones
from before the first bake, also after saving. Only graphs with a surface can be baked.

## Editing

- Previews: `Key previews` shows pictures on texture, procedural, normal, coordinate and logic nodes, `All previews` on all of them.
  They show the values of the opened material.
- A link dropped on the canvas opens the node menu with the nodes it can connect to. In the menu the up and down arrows move through
  the list, left and right close and open categories and Enter adds the node.
- `Ctrl+G` puts a frame around the selection, frames move their nodes along. A double click on a link adds a reroute point.
- `Maximize` (F11) shows the graph over the whole editor, `F` frames the selection or all nodes, `Arrange` lays out everything.
- The `Generated code` window shows the shaders. Variables are named after nodes and parameters, e.g. `fractalNoiseResult`.

