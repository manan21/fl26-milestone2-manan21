#include "aiws/processing_core.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct StrategyStats {
    int calls{};
    int destructions{};
};

class MarkerChunker final : public aiws::ChunkingStrategy {
public:
    explicit MarkerChunker(std::shared_ptr<StrategyStats> stats)
        : stats_(std::move(stats)) {}
    ~MarkerChunker() override { ++stats_->destructions; }

    std::vector<aiws::Chunk> chunk(const aiws::Document& document,
                                   std::size_t document_order) const override {
        ++stats_->calls;
        return {{document.id() + "#marker", document.id(), document_order,
                 0, "marker " + document.text(), 2, 0, document.text().size()}};
    }

private:
    std::shared_ptr<StrategyStats> stats_;
};

class FixedRetriever final : public aiws::RetrievalStrategy {
public:
    explicit FixedRetriever(std::shared_ptr<StrategyStats> stats)
        : stats_(std::move(stats)) {}
    ~FixedRetriever() override { ++stats_->destructions; }

    std::vector<aiws::SearchResult> search(
        const std::string&, int k, const std::vector<aiws::Chunk>& chunks,
        const aiws::CorpusIndex&) const override {
        ++stats_->calls;
        if (k <= 0 || chunks.empty()) return {};
        const auto& chunk = chunks.front();
        return {{chunk.id, chunk.document_id, chunk.sequence, chunk.text, 73.0, 1}};
    }

private:
    std::shared_ptr<StrategyStats> stats_;
};

class MarkingContext final : public aiws::ContextStrategy {
public:
    explicit MarkingContext(std::shared_ptr<StrategyStats> stats)
        : stats_(std::move(stats)) {}
    ~MarkingContext() override { ++stats_->destructions; }

    std::vector<aiws::ContextItem> build(
        const std::vector<aiws::SearchResult>& ranked,
        std::size_t token_budget) const override {
        ++stats_->calls;
        if (ranked.empty() || token_budget == 0) return {};
        const auto& result = ranked.front();
        return {{result.chunk_id, result.document_id, result.chunk_sequence,
                 "custom context", 1, result.score, true}};
    }

private:
    std::shared_ptr<StrategyStats> stats_;
};

void test_default_compatibility() {
    aiws::Workspace workspace;
    workspace.add_document(aiws::Document{"alpha", "", "Alpha beta beta."});
    workspace.add_document(aiws::Document{"gamma", "", "Gamma alpha."});

    aiws::ProcessingCore core;
    core.rebuild(workspace);
    const auto results = core.search("beta", 2);

    check(aiws::ProcessingCore::normalize("Search... SEARCH!! 42-times") ==
              "search search 42 times",
          "default text normalization remains M1-compatible");
    check(core.chunk_count() == 2, "default chunking creates expected chunks");
    check(results.size() == 1 && results.front().document_id == "alpha",
          "default retrieval finds the matching document");
}

void test_custom_dispatch_rollback_and_lifetime() {
    static_assert(!std::is_copy_constructible<aiws::ProcessingCore>::value,
                  "ProcessingCore must not be copy constructible");
    static_assert(!std::is_copy_assignable<aiws::ProcessingCore>::value,
                  "ProcessingCore must not be copy assignable");
    static_assert(std::is_move_constructible<aiws::ProcessingCore>::value,
                  "ProcessingCore must be move constructible");
    static_assert(std::is_move_assignable<aiws::ProcessingCore>::value,
                  "ProcessingCore must be move assignable");

    auto chunk_stats = std::make_shared<StrategyStats>();
    auto retrieval_stats = std::make_shared<StrategyStats>();
    auto context_stats = std::make_shared<StrategyStats>();
    {
        aiws::ProcessingCore core(
            std::make_unique<MarkerChunker>(chunk_stats),
            std::make_unique<FixedRetriever>(retrieval_stats),
            std::make_unique<MarkingContext>(context_stats));
        aiws::Workspace workspace;
        workspace.add_document(aiws::Document{"doc", "", "original text"});
        core.rebuild(workspace);
        check(core.chunks().front().id == "doc#marker",
              "injected chunker output is used during rebuild");

        const auto results = core.search("not present", 1);
        check(results.size() == 1 && results.front().score == 73.0,
              "injected retriever controls search results");
        const auto context = core.build_context("not present", 1, 5);
        check(context.size() == 1 && context.front().text == "custom context",
              "injected context strategy controls context output");
        check(chunk_stats->calls == 1 && retrieval_stats->calls == 2 &&
                  context_stats->calls == 1,
              "each configured strategy is dispatched through ProcessingCore");

        aiws::Workspace duplicate_workspace;
        duplicate_workspace.add_document(aiws::Document{"same", "", "first"});
        duplicate_workspace.add_document(aiws::Document{"same", "", "second"});
        bool duplicate_rejected = false;
        try {
            core.rebuild(duplicate_workspace);
        } catch (const std::invalid_argument&) {
            duplicate_rejected = true;
        }
        check(duplicate_rejected, "duplicate document IDs are rejected");
        check(core.chunks().size() == 1 && core.chunks().front().id == "doc#marker",
              "failed rebuild preserves the previous corpus");

        aiws::ProcessingCore moved(std::move(core));
        check(moved.search("still configured", 1).front().score == 73.0,
              "moved core retains ownership and strategy behavior");
        aiws::ProcessingCore assigned;
        assigned = std::move(moved);
        check(assigned.search("still configured", 1).front().score == 73.0,
              "move assignment retains strategy behavior");
        check(chunk_stats->destructions == 0 && retrieval_stats->destructions == 0 &&
                  context_stats->destructions == 0,
              "strategies remain alive while the moved-to core owns them");
    }
    check(chunk_stats->destructions == 1 && retrieval_stats->destructions == 1 &&
              context_stats->destructions == 1,
          "each transferred strategy is destroyed exactly once");
}

void test_null_strategy_rejected() {
    bool rejected = false;
    try {
        aiws::ProcessingCore core(nullptr, std::make_unique<FixedRetriever>(
                                                 std::make_shared<StrategyStats>()),
                                  std::make_unique<MarkingContext>(
                                      std::make_shared<StrategyStats>()));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "null strategy configuration throws invalid_argument");
}

}  // namespace

int main() {
    test_default_compatibility();
    test_custom_dispatch_rollback_and_lifetime();
    test_null_strategy_rejected();
    if (failures != 0) return 1;
    std::cout << "Student M2 tests passed\n";
    return 0;
}
