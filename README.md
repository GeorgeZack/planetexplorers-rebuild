# Planet Explorers — C Rebuild

A community C11 reconstruction of **Planet Explorers**, based on the publicly released [Pathea Games source project](https://github.com/pathea-games/planetexplorers). **Planet Explorers and its original game assets belong to Pathea Games LLC.** This rebuild is maintained by [GeorgeZack](https://github.com/GeorgeZack); it is not an official Pathea Games release or an endorsed project.

The native implementation is in [`rebuild/`](rebuild/README.md). The original Unity source is not copied here: it is the pinned [`reference/unity`](https://github.com/pathea-games/planetexplorers) submodule, used read-only as the replication reference. Clone with `git clone --recurse-submodules`, or run `git submodule update --init` afterwards. Start with the [project map](PROJECT_MAP.md), [build/run instructions](rebuild/README.md), and [credits and notices](NOTICE.md).

Recovered runtime asset exports, installed Steam game files, build outputs and local saves are excluded from this repository. Some tests and graphical modes require separately recovered local assets; see the build instructions. The documentation includes reference screenshots and asset provenance for this non-commercial reconstruction.

## Original upstream terms

The original README terms below are preserved. No replacement permissive license is asserted for the upstream game, source or assets.

The Planet Explorers Source Project Terms of Use

As of Oct 29th 2019, Pathea Games LLC has made the source code of Planet Explorers public. By downloading any portion of this project you agree to the terms in this document.

All assets under this project remain property of Pathea Games. Direct resales of recompilation is prohibited. You are encouraged to use them in non-commercial or hobbyist projects. If you plan to use them in a commercial project, a written consent from Pathea Games is required. This applies to any modified and derivative versions of the assets.

As for the code portion of the project, Pathea Games LLC does not provide technical support for such and is not responsible for any damage caused by use of the code.

All third-party plugins are intentionally excluded to avoid copyright issues. Pathea Games will not provide the plugins.

## C rebuild

Start with the [project map](PROJECT_MAP.md) and [agent guidance](AGENTS.md) for future work. The [local structural knowledge graph](docs/KNOWLEDGE_GRAPH.md) provides queryable source, documentation and Unity metadata relationships.

The [replication specification](docs/REPLICATION.md) documents the Unity source audit and the plan for reconstruction. The new [C rebuild](rebuild/README.md) contains a playable first slice, build instructions, tests, and its current limitations. The original project and terms above are preserved.

The [automated reference playtest](docs/PLAYTEST.md) records observed tutorial behavior, source-driven corrections, and recovered room rendering.
