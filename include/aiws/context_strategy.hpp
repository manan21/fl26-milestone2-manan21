#pragma once

#include "aiws/processing_types.hpp"

#include <cstddef>
#include <vector>

namespace aiws {

// M2 PUBLIC-INTERFACE DESIGN TASK
// Complete this class as a safe abstract polymorphic interface.
// Keep the class name, operation name, parameter types, return type,
// const qualification, and namespace unchanged.
class ContextStrategy {
public:
    virtual ~ContextStrategy() = default;

    virtual std::vector<ContextItem> build(const std::vector<SearchResult>&,
                                           std::size_t) const = 0;
};

}  // namespace aiws
