#pragma once

#include "aiws/corpus_index.hpp"
#include "aiws/processing_types.hpp"

#include <string>
#include <vector>

namespace aiws {

// M2 PUBLIC-INTERFACE DESIGN TASK
// Complete this class as a safe abstract polymorphic interface.
// Keep the class name, operation name, parameter types, return type,
// const qualification, and namespace unchanged.
class RetrievalStrategy {
public:
    virtual ~RetrievalStrategy() = default;

    virtual std::vector<SearchResult> search(const std::string&,
                                             int,
                                             const std::vector<Chunk>&,
                                             const CorpusIndex&) const = 0;
};

}  // namespace aiws
