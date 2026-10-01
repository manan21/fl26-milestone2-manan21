#pragma once

#include "aiws/document.hpp"
#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

// M2 PUBLIC-INTERFACE DESIGN TASK
// Complete this class as a safe abstract polymorphic interface.
// Keep the class name, operation name, parameter types, return type,
// const qualification, and namespace unchanged.
class ChunkingStrategy {
public:
    virtual ~ChunkingStrategy() = default;

    virtual std::vector<Chunk> chunk(const Document&,
                                     std::size_t) const = 0;
};

}  // namespace aiws
