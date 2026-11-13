# SAVE FORMAT V9 — v0.1

## 1. OVERVIEW

Save format version 9 adds mission system state to the save format.

## 2. SECTIONS

The save format consists of the following sections in order:

1. Runtime
2. Generated
3. Entities
4. Districts
5. Territories
6. Factions
7. Npcs
8. Vehicles
9. Missions
10. Relationships
11. FactionSimulation
12. NpcSimulation
13. TerritorySimulation
14. MarketSimulation
15. StrategicHistory
16. StoryGraph
17. MissionSimulation

## 3. MISSION SIMULATION SECTION

The mission simulation section contains:

- Version (current: 1)
- Configuration:
  - maxActiveMissions
  - maxAvailableMissions
- Statistics:
  - missionsCreated
  - missionsAccepted
  - missionsActive
  - missionsCompleted
  - missionsFailed
  - missionsExpired
  - missionsInvalidated
  - missionsResolvedByNpc
- Next mission ID
- Mission count
- For each mission:
  - Mission ID
  - Template ID (string)
  - Category
  - Status (MissionState)
  - Issuer:
    - Kind (Npc, Faction, Systemic, Unknown)
    - Npc ID (if kind == Npc)
    - Faction ID (if kind == Faction)
    - Hidden flag
  - Context:
    - Target Npc ID
    - Target Faction ID
    - Target Territory ID
    - Target District ID
    - Payload Item ID
    - Recipient Npc ID
  - Timestamps:
    - Created at (game time)
    - Expires at (optional, game time)
  - Reward:
    - Cash (signed 64-bit)
    - Respect (signed 32-bit)
    - Faction Favor (FactionId)
    - Favor Amount (signed 32-bit)
  - Objective definitions count
  - For each objective definition:
    - Objective ID
    - Type (ObjectiveType)
    - Description (string)
    - Target Npc ID
    - Target Faction ID
    - Target Territory ID
    - Target District ID
    - Payload Item ID
    - Recipient Npc ID
    - Optional flag
    - Transitions count
    - For each transition:
      - Trigger (OnSuccess, OnFailure)
      - Action (ActivateObjective, SkipObjective, CompleteMission, FailMission)
      - Target Objective ID
  - Objective states count
  - For each objective state:
    - Objective ID
    - State (ObjectiveState)
    - Started at (game time, 0 if not started)
    - Completed at (game time, 0 if not completed)
    - Location reached flag
    - Interaction occurred flag
    - Delivery completed flag
    - Protected resolved flag
    - Attack resolved flag

All integers are little-endian. Strings are length-prefixed with a 32-bit
length followed by UTF-8 bytes.

## 4. STATE HASH

The mission simulation state contributes to StateHash v11 with the following
data in order:

- Version
- Configuration (maxActiveMissions, maxAvailableMissions)
- Statistics (missionsCreated, missionsAccepted, missionsActive,
  missionsCompleted, missionsFailed, missionsExpired, missionsInvalidated,
  missionsResolvedByNpc)
- Next mission ID
- Mission count
- For each mission (in deterministic order by mission ID):
  - Mission ID value
  - Template ID string
  - Category value
  - Status value
  - Issuer kind value
  - Issuer Npc ID value (0 if not Npc)
  - Issuer Faction ID value (0 if not Faction)
  - Issuer hidden flag (0 or 1)
  - Context target Npc ID value
  - Context target Faction ID value
  - Context target Territory ID value
  - Context target District ID value
  - Context payload item ID
  - Context recipient Npc ID value
  - Created at milliseconds
  - Expires at present flag (0 or 1)
  - Expires at milliseconds (if present)
  - Reward cash value (signed 64-bit)
  - Reward respect value (signed 32-bit)
  - Reward favor faction ID value
  - Reward favor amount value (signed 32-bit)
  - Objective definitions count
  - For each objective definition (in deterministic order by objective ID):
    - Objective ID value
    - Type value
    - Description string
    - Target Npc ID value
    - Target Faction ID value
    - Target Territory ID value
    - Target District ID value
    - Payload item ID
    - Recipient Npc ID value
    - Optional flag (0 or 1)
    - Transitions count
    - For each transition:
      - Trigger value
      - Action value
      - Target objective ID value
  - Objective states count
  - For each objective state (in deterministic order by objective ID):
    - Objective ID value
    - State value
    - Started at milliseconds
    - Completed at milliseconds
    - Location reached flag (0 or 1)
    - Interaction occurred flag (0 or 1)
    - Delivery completed flag (0 or 1)
    - Protected resolved flag (0 or 1)
    - Attack resolved flag (0 or 1)

## 5. MIGRATION

Save format version 9 migrates from version 8 by:
- Keeping all existing sections unchanged
- Adding the new StoryGraph and MissionSimulation sections at the end
- Setting saveFormatVersion to 9

When loading a version 8 save, the MissionSimulation section is absent and
defaults to an uninitialized state (version 0, empty mission store).