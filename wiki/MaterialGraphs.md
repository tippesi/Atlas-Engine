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

**Opacity:** an `Opacity` that varies over the surface cuts out fragments, which turns off early depth tests and costs time even
where nothing is cut out. An `Opacity` that only depends on parameters and constants, e.g. a fade or visibility switch per
material, is evaluated per vertex instead and hides the whole draw, the fragments stay as fast as opaque ones.

## Limits

- Opaque and cutout surfaces only, no transparency. The lighting model is the regular one, graphs write into the G-buffer.
- Ray tracing doesn't run graphs: reflections, GI, ray traced shadows and the path tracer see the mesh without displacement and
  the regular material properties, which act as the fallback.
- Impostors, vegetation clutter and terrain don't use graphs.
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

Capture jobs build and change graphs with the edit operation `materialGraph` and put them on materials with
`assignMaterialGraph`, `check` finds wrong node types, pins and compile errors without loading anything
(`src/capture/README.md`). `AtlasEngineCapture inspect nodes` lists the node library with its pins, `inspect <graph>`
the nodes of a graph with their links and stages.

## Sharing a graph

A graph is its own resource, any number of materials can reference it. Pipelines are shared, values and textures of parameters
belong to each material. When other materials use the graph, the editor shows them above the canvas, since changing nodes changes
all of them. `Make a copy for this material` gives the material its own graph file.

## Parameters

Parameter nodes have a range (materials get a slider), a group (a heading in the material) and an order, all in the inspector next
to the canvas. Without an order colors come first, then values, then textures. Texture parameters also have a default texture, which
materials use until they select their own.

## Baking for ray tracing

Reflections, global illumination, ray traced shadows and the path tracer don't run graphs, they use the properties of the material.
`Bake for ray tracing` in the material window (or the capture operation `bakeMaterial`) writes the surface of the graph into them:

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

