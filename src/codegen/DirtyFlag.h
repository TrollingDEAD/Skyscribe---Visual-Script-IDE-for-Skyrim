#pragma once

namespace codegen {

// Lightweight dirty flag. Set when the graph changes; cleared after codegen runs.
class DirtyFlag {
public:
    void Set()   { dirty_ = true; }
    void Clear() { dirty_ = false; }
    bool IsSet() const { return dirty_; }

private:
    bool dirty_ = true; // start dirty so first render triggers codegen
};

} // namespace codegen
