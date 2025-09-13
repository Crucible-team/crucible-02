# Changelog

## 0.72.0

- Added EntityOutputsComponent: a simple, serializable component to define Source-style entity outputs in rows. Each row contains: `event`, `target`, `input`, `parameter`, `delay` (seconds), and `once` (bool). Edited via the Editor’s "Entity Outputs" window. Not exposed to Lua yet.
- MetadataComponent behavior updated: values are now stored as ordered, per-type maps with unique keys within each type. Setting a value with an existing name overwrites the previous entry; insertion order is preserved; removing an entry reindexes correctly. Lua API remains the same (Has/Get/Set for bool/int/float/string and Preset).
- Engine version bumped to 0.72.0.

