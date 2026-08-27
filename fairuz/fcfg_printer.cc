// cfg_print.hpp
//
// A CFG pretty printer that actually draws a graph, instead of dumping
// per-block predecessor/successor lists.
//
//   printCfgAscii(program, std::cout);   // layered ASCII diagram in the terminal
//   printCfgDot(program, std::cout);     // Graphviz DOT -> `dot -Tsvg cfg.dot -o cfg.svg`
//
// Both share one layout pass (computeLayout) so what you see in the terminal
// matches the shape you'd get from Graphviz, just cruder.
//
// Design notes:
//  - Blocks are assigned to layers via BFS from the entry block. Back-edges
//    (edges into an already-visited/ancestor layer -- i.e. loop edges) are
//    detected separately and rendered differently (↺ / dashed in DOT) rather
//    than drawn as if they were normal forward control flow.
//  - Within a layer, blocks are ordered to minimize edge crossings using a
//    simple barycenter heuristic (median of predecessor positions), refined
//    over a couple of passes -- this is the same idea Sugiyama-style layered
//    graph drawers use, just without a real crossing-minimization solver.
//  - Unreachable blocks (no path from entry) are detected and called out
//    explicitly instead of silently appearing in the block list.

#include "fcfg_printer.hpp"
#include "fcfg.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace fairuz::cfgprint {

std::string terminatorName(TerminatorTag tag)
{
    switch (tag) {
    case TerminatorTag::BRANCH: return "BRANCH";
    case TerminatorTag::JUMP: return "JUMP";
    case TerminatorTag::NORETURN: return "NORETURN";
    case TerminatorTag::RETURN: return "RETURN";
    case TerminatorTag::NONE: return "NONE";
    }
}

// ---------------------------------------------------------------------------
// Shared analysis: layering + edge classification
// ---------------------------------------------------------------------------

// BFS layering from entry, classify every succ edge, then untangle ordering
// within each layer with a couple of barycenter passes.
 Layout computeLayout(Fa_CFG const* cfg)
{
    assert(cfg != nullptr);

    Layout out;
    if (cfg->blocks.empty())
        return out;

    Fa_BasicBlock const* entry = cfg->entry ? cfg->entry : cfg->blocks.front();

    // BFS assigns each reachable block its shortest-path layer from entry.
    // This is what makes the diagram read top-to-bottom like control flow,
    // rather than in raw block-id or vector order.
    std::unordered_map<Fa_BasicBlock const*, int> layer;
    std::deque<Fa_BasicBlock const*> queue;
    layer[entry] = 0;
    queue.push_back(entry);
    while (!queue.empty()) {
        Fa_BasicBlock const* b = queue.front();
        queue.pop_front();
        int nextLayer = layer[b] + 1;
        for (Fa_BasicBlock const* s : b->succs) {
            if (layer.find(s) == layer.end()) {
                layer[s] = nextLayer;
                queue.push_back(s);
            }
        }
    }

    // Anything BFS never reached is dead/unreachable code -- report it
    // separately rather than letting it masquerade as part of the flow.
    std::unordered_set<Fa_BasicBlock const*> reachable;
    for (auto const& [b, l] : layer)
        reachable.insert(b);
    for (Fa_BasicBlock const* b : cfg->blocks) {
        if (!reachable.count(b))
            out.unreachable.push_back(b);
    }

    // Classify edges: a forward edge increases layer; anything that goes to
    // an equal-or-lower layer among reachable blocks is a back-edge (loop),
    // matching the standard DFS/BFS back-edge definition for reducible CFGs.
    for (Fa_BasicBlock const* b : cfg->blocks) {
        if (!reachable.count(b))
            continue;
        for (Fa_BasicBlock const* s : b->succs) {
            EdgeKind kind;
            if (!reachable.count(s))
                kind = EdgeKind::CrossOrUnreachable;
            else if (layer[s] > layer[b])
                kind = EdgeKind::Forward;
            else
                kind = EdgeKind::Back;
            out.edges.push_back(Edge { b, s, kind });
        }
    }

    int maxLayer = 0;
    for (auto const& [b, l] : layer)
        maxLayer = std::max(maxLayer, l);
    out.layers.resize(maxLayer + 1);
    for (auto const& [b, l] : layer)
        out.layers[l].push_back(b);
    out.layer = std::move(layer);

    // Stable initial order within a layer: by block id, so re-runs on the
    // same CFG are deterministic before we start reordering.
    for (auto& row : out.layers) {
        std::sort(row.begin(), row.end(),
            [](auto const* a, auto const* b) { return a->id < b->id; });
    }

    // Barycenter crossing-reduction: a few sweeps down then up, each time
    // reordering a layer by the average column position of its neighbors
    // in the adjacent layer. Cheap, no external deps, good enough for the
    // block counts a single function's CFG realistically has.
    auto columnOf = [&](Fa_BasicBlock const* b) {
        auto& row = out.layers[out.layer[b]];
        return static_cast<double>(std::find(row.begin(), row.end(), b) - row.begin());
    };

    auto reorderLayer = [&](int li, bool useSuccs) {
        auto& row = out.layers[li];
        std::vector<std::pair<double, Fa_BasicBlock const*>> keyed;
        keyed.reserve(row.size());
        for (Fa_BasicBlock const* b : row) {
            auto const& neighbors = useSuccs ? b->succs : b->preds;
            double sum = 0;
            int n = 0;
            for (Fa_BasicBlock const* nb : neighbors) {
                auto it = out.layer.find(nb);
                if (it == out.layer.end())
                    continue;
                int wantLayer = useSuccs ? li + 1 : li - 1;
                if (it->second != wantLayer)
                    continue; // only weight immediate-neighbor layers
                sum += columnOf(nb);
                n += 1;
            }
            double key = n > 0 ? sum / n : columnOf(b);
            keyed.push_back({ key, b });
        }
        std::stable_sort(keyed.begin(), keyed.end(),
            [](auto const& a, auto const& b) { return a.first < b.first; });
        for (size_t i = 0; i < row.size(); i += 1)
            row[i] = keyed[i].second;
    };

    for (int pass = 0; pass < 3; pass += 1) {
        for (int li = 1; li < static_cast<int>(out.layers.size()); li += 1)
            reorderLayer(li, /*useSuccs=*/false);
        for (int li = static_cast<int>(out.layers.size()) - 2; li >= 0; li -= 1)
            reorderLayer(li, /*useSuccs=*/true);
    }

    return out;
}

// ---------------------------------------------------------------------------
// ASCII graph rendering
// ---------------------------------------------------------------------------

namespace detail {

 std::string blockLabel(Fa_BasicBlock const& b)
{
    std::ostringstream os;
    os << "B" << b.id << " [" << terminatorName(b.terminator) << "]"
       << " (" << b.stmts.size() << (b.stmts.size() == 1 ? " stmt" : " stmts") << ")";
    return os.str();
}

} // namespace detail

// Renders each layer as a row of boxed blocks, with an edge summary line
// between rows naming which box feeds which (since ASCII line-drawing for
// arbitrary many-to-many crossing edges gets unreadable fast, we draw the
// straight/adjacent cases as connectors and spell out any edge that would
// otherwise cross layers or skip a rank).
 void printCfgAscii(Fa_CFG const* cfg, std::ostream& os)
{
    assert(cfg != nullptr);

    Layout layout = computeLayout(cfg);

    if (cfg->blocks.empty()) {
        os << "  (empty cfg)\n";
        return;
    }

    // Precompute a display box per block. The entry marker is baked into
    // the label up front so box width already accounts for it -- computing
    // width from the bare label and prepending "-> " later would make the
    // entry box's border narrower than its own text.
    std::unordered_map<Fa_BasicBlock const*, detail::Box> boxes;
    for (Fa_BasicBlock const* b : cfg->blocks) {
        std::string label = detail::blockLabel(*b);
        if (b == cfg->entry)
            label = "-> " + label;
        boxes[b] = detail::Box { b, label, static_cast<int>(label.size()) + 2 };
    }

    // Fast lookup: successors annotated with their edge kind, in the same
    // left-to-right order we'll print them, so the "-> B3, B5(back)" line
    // matches what's visually below/above.
    auto succEdgesOf = [&](Fa_BasicBlock const* b) {
        std::vector<Edge const*> es;
        for (auto const& e : layout.edges) {
            if (e.from == b)
                es.push_back(&e);
        }
        return es;
    };

    for (int li = 0; li < static_cast<int>(layout.layers.size()); li += 1) {
        auto const& row = layout.layers[li];

        // Top border
        for (Fa_BasicBlock const* b : row)
            os << "┌" << std::string(boxes[b].width, '-') << "┐  ";
        os << "\n";

        // Label line (entry marker, if any, is already part of the label).
        // width = label.size() + 2, so one leading + one trailing space
        // exactly fills the box when pad == 1; compute it explicitly rather
        // than re-deriving it ad hoc to avoid drifting off by one again.
        for (Fa_BasicBlock const* b : row) {
            std::string const& label = boxes[b].label;
            int innerWidth = boxes[b].width;                           // space between the two '|' chars
            int pad = innerWidth - static_cast<int>(label.size()) - 1; // 1 leading space
            os << "| " << label << std::string(std::max(0, pad), ' ') << "|  ";
        }
        os << "\n";

        // Bottom border
        for (Fa_BasicBlock const* b : row)
            os << "└" << std::string(boxes[b].width, '-') << "┘  ";
        os << "\n";

        // Outgoing edges from this row, written as a compact arrow list so
        // fan-out/fan-in and back-edges are all legible regardless of how
        // many boxes are on the row -- this is the piece a pure box grid
        // can't express on its own.
        bool anyEdge = false;
        for (Fa_BasicBlock const* b : row) {
            auto edges = succEdgesOf(b);
            if (edges.empty())
                continue;
            anyEdge = true;
            os << "  B" << b->id << " -> ";
            for (size_t i = 0; i < edges.size(); i += 1) {
                Edge const* e = edges[i];
                os << "B" << e->to->id;
                if (e->kind == EdgeKind::Back)
                    os << " (loop back-edge ↺)";
                else if (e->kind == EdgeKind::CrossOrUnreachable)
                    os << " (-> unreachable code)";
                if (i + 1 < edges.size())
                    os << ", ";
            }
            os << "\n";
        }
        if (anyEdge)
            os << "\n";
        else
            os << "\n";
    }

    if (!layout.unreachable.empty()) {
        os << "  ! unreachable blocks (no path from entry): ";
        for (size_t i = 0; i < layout.unreachable.size(); i += 1) {
            os << "B" << layout.unreachable[i]->id;
            if (i + 1 < layout.unreachable.size())
                os << ", ";
        }
        os << "\n\n";
    }
}

// ---------------------------------------------------------------------------
// Graphviz DOT export -- the version worth using once a CFG has more than a
// handful of blocks or any nontrivial loop nesting; ASCII layout is a
// heuristic, `dot` does real crossing minimization.
// ---------------------------------------------------------------------------

 void printCfgDot(Fa_CFG const* cfg, std::ostream& os, std::string_view graphName = "cfg")
{
    assert(cfg != nullptr);

    Layout layout = computeLayout(cfg);

    os << "digraph " << graphName << " {\n";
    os << "  rankdir=TB;\n";
    os << "  node [shape=box, fontname=\"monospace\", fontsize=10];\n";
    os << "  edge [fontname=\"monospace\", fontsize=9];\n\n";

    for (Fa_BasicBlock const* b : cfg->blocks) {
        os << "  B" << b->id << " [label=\"" << detail::blockLabel(*b) << "\"";
        if (b == cfg->entry)
            os << ", style=filled, fillcolor=lightgreen";
        if (std::find(layout.unreachable.begin(), layout.unreachable.end(), b)
            != layout.unreachable.end())
            os << ", style=filled, fillcolor=lightgray";
        os << "];\n";
    }
    os << "\n";

    for (auto const& e : layout.edges) {
        os << "  B" << e.from->id << " -> B" << e.to->id;
        switch (e.kind) {
        case EdgeKind::Back:
            os << " [style=dashed, color=firebrick, label=\"loop\", constraint=false]";
            break;
        case EdgeKind::CrossOrUnreachable:
            os << " [style=dotted, color=gray]";
            break;
        case EdgeKind::Forward:
            break;
        }
        os << ";\n";
    }

    os << "}\n";
}

// ---------------------------------------------------------------------------
// Top-level driver: one function/script's header + both/either view.
// ---------------------------------------------------------------------------

void printCfg(fairuz::Fa_Program const& program, std::ostream& os, Mode mode)
{
    auto const& functions = program.get_functions();
    for (size_t fi = 0; fi < functions.size(); fi += 1) {
        auto const* fn = functions[fi];
        std::string_view name = fn->def == nullptr ? "<script>" : "<function>";

        os << "=== CFG: " << name << " (entry B"
           << (fn->cfg->entry ? fn->cfg->entry->id : 0) << ", "
           << fn->cfg->blocks.size() << " blocks) ===\n\n";

        if (mode == Mode::ASCII || mode == Mode::BOTH)
            printCfgAscii(fn->cfg, os);

        if (mode == Mode::DOT || mode == Mode::BOTH) {
            std::ostringstream graphName;
            graphName << "fn" << fi;
            printCfgDot(fn->cfg, os, graphName.str());
            os << "\n";
        }
    }
}

} // namespace fairuz::cfgprint
