#ifndef FA_CFG_PRINTER_HPP
#define FA_CFG_PRINTER_HPP

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

#include "fforward.hpp" // Fa_Program, Fa_CFG, Fa_BasicBlock, terminatorName()

#include <unordered_map>
#include <vector>
#include <iostream>

namespace fairuz::cfgprint {

// ---------------------------------------------------------------------------
// Shared analysis: layering + edge classification
// ---------------------------------------------------------------------------

enum class EdgeKind { Forward,
    Back,
    CrossOrUnreachable };

struct Edge {
    Fa_BasicBlock const* from;
    Fa_BasicBlock const* to;
    EdgeKind kind;
};

struct Layout {
    // layer[block] = distance-from-entry layer index
    std::unordered_map<Fa_BasicBlock const*, int> layer;
    // order of blocks within each layer, left-to-right
    std::vector<std::vector<Fa_BasicBlock const*>> layers;
    std::vector<Edge> edges;
    std::vector<Fa_BasicBlock const*> unreachable;
};

// BFS layering from entry, classify every succ edge, then untangle ordering
// within each layer with a couple of barycenter passes.
Layout computeLayout(Fa_CFG const* cfg);

// ---------------------------------------------------------------------------
// ASCII graph rendering
// ---------------------------------------------------------------------------

namespace detail {

std::string blockLabel(Fa_BasicBlock const& b);

struct Box {
    Fa_BasicBlock const* block;
    std::string label;
    int width;
};

} // namespace detail

// Renders each layer as a row of boxed blocks, with an edge summary line
// between rows naming which box feeds which (since ASCII line-drawing for
// arbitrary many-to-many crossing edges gets unreadable fast, we draw the
// straight/adjacent cases as connectors and spell out any edge that would
// otherwise cross layers or skip a rank).
void printCfgAscii(Fa_CFG const& cfg, std::ostream& os);

// ---------------------------------------------------------------------------
// Graphviz DOT export -- the version worth using once a CFG has more than a
// handful of blocks or any nontrivial loop nesting; ASCII layout is a
// heuristic, `dot` does real crossing minimization.
// ---------------------------------------------------------------------------

void printCfgDot(Fa_CFG const& cfg, std::ostream& os, std::string_view graphName = "cfg");

// ---------------------------------------------------------------------------
// Top-level driver: one function/script's header + both/either view.
// ---------------------------------------------------------------------------

enum class Mode { 
    ASCII,
    DOT,
    BOTH, 
};

void printCfg(fairuz::Fa_Program const& program, std::ostream& os = std::cout,
    Mode mode = Mode::BOTH);

} // namespace fairuz::cfgprint

#endif // FA_CFG_PRINTER_HPP