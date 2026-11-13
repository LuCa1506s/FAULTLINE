# MISSION CORE — v0.1

## 1. OVERVIEW

The Mission Core provides a generic mission system that supports player-facing
opportunities derived from world state. It is separate from the Story Graph
which defines high-level narrative possibilities.

## 2. MISSION STATES

The mission state machine consists of exactly eight states:

```
AVAILABLE
ACCEPTED
ACTIVE
COMPLETED
FAILED
EXPIRED
INVALIDATED
RESOLVED_BY_NPC
```

Transitions are validated to prevent illegal state changes.

## 3. OBJECTIVE GRAPH

Missions are composed of objective graphs, not linear checklists. Each
objective has a type, start condition, success condition, failure condition,
and transitions to other objectives on success or failure.

Supported objective types for Phase 15:
- GO_TO: reach a location
- INTERACT: interact with an NPC
- DELIVER: deliver an item to an NPC
- PROTECT: ensure an NPC survives
- ATTACK: ensure an NPC is neutralized

## 4. TYPED IDS

- MissionId: unique identifier for a mission instance
- ObjectiveId: unique identifier for an objective definition

## 5. MISSION DEFINITION VS INSTANCE

- MissionDefinition: immutable template containing objective graph,
  conditions, transitions, rewards, expiration, and issuer context.
- MissionInstance: runtime state including mission ID, current state,
  resolved context, objective runtime states, and timestamps.

## 6. EVENT-DRIVEN ARCHITECTURE

The Mission System observes domain events from other systems:
- TerritoryControlChanged
- FactionOperationResolved
- NpcDied

It emits mission events:
- MissionAccepted, MissionActivated, MissionCompleted, MissionFailed,
  MissionExpired, MissionInvalidated, MissionResolvedByNpc
- ObjectiveCompleted, ObjectiveFailed

## 7. STORYGRAPH BRIDGE

A mission may optionally originate from a StoryNode. When a mission
completes, fails, or is resolved by an NPC, the associated StoryNode
may be updated accordingly.

## 8. PERSISTENCE

Mission state is persisted with SaveFormatVersion 9 and participates in
StateHash v11 with canonical deterministic ordering.

## 9. VALIDATION

Mission definitions and instances are validated to ensure:
- Referenced IDs exist
- Objective graph is reachable
- At least one terminal resolution exists
- No illegal state transitions
- Proper condition and transition structure

## 10. DETERMINISM

The Mission System uses no runtime RNG. Save/load preserves deterministic
behavior: identical initial state, mission definition, and event sequence
produce identical final state and save bytes.