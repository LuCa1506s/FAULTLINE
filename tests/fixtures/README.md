# Historical save fixture

`save_v1_reference.flsave` uses the exact published Phase 8 little-endian v1
format, including a faction, leader NPC, territory, mission, old allocator marks,
clock and all RNG words. It intentionally has no generated baseline so it stays
small and independently readable. Normal tests only read this fixed binary.

SHA-256: `db23ea06ef1008bbe82674d1f3a6e37abea991d2d60087c596488d6a364d4a3c`.

`create_v1_reference.ps1` records its explicit primitive/field encoding without
calling the current C++ serializer. Run it only when intentionally maintaining
fixture provenance; do not regenerate fixtures to hide compatibility failures.
The migration test checks preserved semantic values and verifies that the frozen
v1 test encoder reproduces the independent fixture byte for byte. Additional
generated-world migration tests exercise all three supported generator versions.

An attempt to generate fixtures through a temporary Phase 8 executable was
blocked by Windows Application Control. That executable was not used, and no
security policy was changed or bypassed. The independent wire fixture and frozen
v1 field encoder provide reproducible coverage without requiring that program.

## Phase 9 v2 fixture

`save_v2_phase9.flsave` is an unmodified real Phase 9 tool save (seed 12345,
generator 3, tick 1152000 / 30 days), copied before Phase 10 changes. It contains
468 operation records, 8 pending operations, all 22 generated major NPCs and
runtime resource/RNG/allocator/scheduler states. Size: 1012765 bytes.
SHA-256: `2e5e5772efdf86446261028aee9679179a01f5a5d328590d3bf187afdc899e92`.
Tests read this fixed v2 binary and verify that all eleven historical section
payloads survive migration byte for byte before exercising resumed simulation.
Do not regenerate the fixture with the current encoder to mask incompatibility.
