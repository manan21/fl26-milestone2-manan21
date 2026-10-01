# M2 Technical & Design Understanding

This file is an assessed technical-understanding artifact, not ordinary project documentation.
Answer all four questions using your own submitted implementation. Concise answers are acceptable when they are technically correct and specific.

Generic descriptions of C++ concepts or restatements of the assignment that do not identify and explain corresponding parts of your code will receive limited credit.

## 1. Polymorphism and dynamic dispatch - 1.5 points

Runtime polymorphism occurs in `ProcessingCore::rebuild`: it calls `impl_->chunking->chunk(doc, order)` through a `std::unique_ptr<ChunkingStrategy>`. The default constructor installs a `Chunker`, while the configurable constructor can install a student-defined subclass. Since `chunk` is virtual and overridden, C++ dispatches to the dynamic object's implementation. If `chunk` were not virtual, a call through the base interface would not dispatch to the derived override, so custom chunkers would not provide their chunking behavior.

## 2. Ownership and lifetime - 1.5 points

The default `ProcessingCore` constructor creates `Chunker`, `RetrievalEngine`, and `ContextBuilder` with `std::make_unique` and delegates to the configurable constructor. A caller can instead create derived strategies with `std::make_unique` and pass those pointers to that same constructor. The pointers move into `ProcessingCore::Impl`, which exclusively owns the strategies. Their `unique_ptr`s destroy them when the owning core is destroyed or when move assignment replaces its current implementation. `ProcessingCore` is move-only because its implementation and strategy ownership are unique and must not be duplicated; move operations transfer that ownership. Each base interface has a virtual destructor so deleting a derived strategy through its base `unique_ptr` runs the complete derived destructor.

## 3. Architecture, extensibility, and M1 compatibility - 1.5 points

`ProcessingCore::Impl` stores one owning pointer for each strategy interface, and `rebuild`, `search`, and `build_context` call those interfaces. This separates coordination and corpus state from the algorithms. The default constructor supplies `Chunker` with `ChunkingPolicy{kMaxChunkTokens, kChunkOverlap, kParagraphPreferenceWindow}`, plus the existing `RetrievalEngine` and `ContextBuilder`, preserving the M1 implementations. A client can pass derived strategies through the configurable constructor and continue to use the same `rebuild`, `search`, and `build_context` API. One alternative is to add `if` branches or flags inside `ProcessingCore` for each algorithm. That would couple the coordinator to every implementation and require changing it to add variants; constructor composition keeps the selection at construction and allows derived strategies without expanding the core's public operations.

## 4. Testing and defect reasoning - 1.5 points

`test_custom_dispatch_rollback_and_lifetime` injects `MarkerChunker`, `FixedRetriever`, and `MarkingContext`, all derived from the published interfaces. It checks for the custom chunk ID, the retriever's fixed score, and the context's marker text, as well as per-strategy call counts. Those outputs would fail if `ProcessingCore` declared strategy interfaces but still called the default concrete algorithms. The same test attempts a duplicate-ID rebuild and confirms that the previous custom chunk remains searchable, detecting a rebuild that overwrites corpus state before validation completes. It then move-constructs and move-assigns the core and checks that the injected strategies remain usable and are destroyed exactly once. This covers substitution, rollback, and ownership behavior not established by merely rerunning the course tests.
