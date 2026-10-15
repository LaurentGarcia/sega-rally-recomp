# Source history and validation revisions

The public history preserves the upstream commits and all source trees. Before
initial publication, the maintainer's author/committer email metadata was changed
to the personal GitHub noreply address. Names, dates, commit messages and source
trees were unchanged; upstream contributor identities and signatures were retained.

Changing commit metadata also changes descendant commit IDs. The
[revision map](revision-map.json) translates original local IDs used in
[HANDOFF.md](../HANDOFF.md) and the [status record](segarally-status.md) to their
public equivalents. Its `mapping` includes only changed commits; unlisted commits
retain their IDs. The original local checkout history and validation artifacts
remain preserved. A source-tree match does not relabel an older binary as a new
build or rerun its tests. Historical app/report hashes still identify those exact
older artifacts, which are not included in the public repository.

The map ends at the source preparation commit. This document and the map are
added afterward, so their own commit does not require a mapped predecessor.
Future development uses the public history directly.
