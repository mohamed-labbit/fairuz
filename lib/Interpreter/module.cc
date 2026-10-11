//
// module.cc
//

#include "Compiler/compiler.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/lexer.hpp"
#include "Parser/parser.hpp"
#include "Platform.hpp"

#include <memory>

namespace fairuz::runtime {

std::filesystem::path VM::resolve_module_path(std::string const& name) const
{
    std::filesystem::path relative;
    size_t begin = 0;
    while (begin < name.size()) {
        size_t dot = name.find('.', begin);
        relative /= platform::path(name.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin));
        if (dot == std::string::npos)
            break;
        begin = dot + 1;
    }
    relative += platform::path(".ف");

    std::vector<std::filesystem::path> roots;
    // Standard-library roots are authoritative for module names they contain.
    // This prevents a project file such as `file.ف` from accidentally
    // shadowing the bundled module. Names absent from these roots still fall
    // through to the importing module's directory for normal relative imports.
    if (auto configured = platform::environment("FAIRUZ_STDLIB"); !configured.empty())
        roots.emplace_back(platform::path(configured));
#ifdef FAIRUZ_SOURCE_STDLIB_DIR
    roots.emplace_back(platform::path(FAIRUZ_SOURCE_STDLIB_DIR));
#endif
#ifdef FAIRUZ_INSTALL_STDLIB_DIR
    roots.emplace_back(platform::path(FAIRUZ_INSTALL_STDLIB_DIR));
#endif
    if (m_frames_top > 0 && !frame().chunk->source_path.empty())
        roots.push_back(platform::path(frame().chunk->source_path).parent_path());
    roots.push_back(std::filesystem::current_path());

    std::error_code ec;
    for (auto const& root : roots) {
        std::filesystem::path candidate = root / relative;
        if (!std::filesystem::is_regular_file(candidate, ec)) {
            ec.clear();
            continue;
        }
        auto canonical = std::filesystem::weakly_canonical(candidate, ec);
        return ec ? candidate.lexically_normal() : canonical;
    }
    return { };
}

ObjModule* VM::load_module(std::string const& name)
{
    std::filesystem::path path = resolve_module_path(name);
    if (path.empty())
        raise_error(ErrorCode::MODULE_NOT_FOUND, "'" + name + "'");
    std::string key = platform::utf8(path);
    if (auto found = m_module_cache.find(key); found != m_module_cache.end())
        return found->second;

    auto globals = std::make_unique<GlobalEnvironment>();
    globals->fallback = &m_builtin_environment;
    GlobalEnvironment* globals_ptr = globals.get();
    m_module_environments.push_back(std::move(globals));

    ObjModule* module = m_gc.make_obj_module(name, key, globals_ptr);
    m_module_cache.emplace(key, module); // publish first so import cycles terminate

    std::unique_ptr<lex::FileManager> source;
    try {
        source = std::make_unique<lex::FileManager>(key);
    } catch (FileError const& error) {
        auto code = error.kind() == FileError::Kind::NotFound ? ErrorCode::FILE_NOT_FOUND
            : error.kind() == FileError::Kind::Read           ? ErrorCode::READ_ERROR
                                                              : ErrorCode::FILE_NOT_OPEN;
        raise_error(code, error.path());
    }
    diagnostic::SourceScope source_scope(source.get());
    parser::Parser parser(source.get());
    Array<AST::StmtPtr> statements = parser.parse_program();
    if (diagnostic::has_errors())
        halt();

    Compiler compiler;
    Chunk* imported_chunk = compiler.compile(statements);
    if (imported_chunk == nullptr || diagnostic::has_errors())
        halt();
    imported_chunk->source_path = key;
    imported_chunk->name = StringRef(module->name.data(), module->name.size());
    module->chunk = imported_chunk;
    m_module_sources.push_back(std::move(source));
    return module;
}

} // fairuz::runtime
