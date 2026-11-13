# RENDERER INTERFACE SPECIFICATION — v0.1

**Project:** Procedural Crime Sandbox  
**File:** `13_RENDERER_INTERFACE_SPEC.md`  
**Parent specs:** `01` through `12`  
**Purpose:** Define a renderer-independent presentation boundary and the first 3D ASCII/ANSI frontend.

---

# 1. CORE PRINCIPLE

Canonical rule:

```text
GRAPHICS MUST NEVER CONTROL GAME LOGIC
```

The simulation stores semantic state.

Renderer decides how to display it.

---

# 2. DATA FLOW

```text
GameState
↓
Presentation Extraction
↓
RenderSnapshot
↓
IRenderer
↓
ASCII Renderer
```

Future:

```text
RenderSnapshot
↓
different renderer
```

without rewriting gameplay.

---

# 3. RENDERER INTERFACE

Conceptually:

```cpp
class IRenderer
{
public:
    virtual void render(const RenderSnapshot&) = 0;
    virtual ~IRenderer() = default;
};
```

Exact API may avoid virtual dispatch later.

Boundary matters more than syntax.

---

# 4. RENDER SNAPSHOT

Renderer receives presentation-safe data.

Conceptually:

```cpp
struct RenderSnapshot
{
    CameraView camera;
    std::vector<RenderObject> objects;
    HudSnapshot hud;
    EnvironmentSnapshot environment;
};
```

---

# 5. NO MUTABLE GAMESTATE

Renderer must not receive writable authoritative state.

Prefer immutable snapshot/view.

---

# 6. RENDER OBJECT

Conceptually:

```cpp
struct RenderObject
{
    RenderObjectId id;
    RenderSemanticType type;

    RenderTransform transform;

    RenderAppearance appearance;

    uint32_t flags;
};
```

---

# 7. SEMANTIC TYPES

Examples:

```text
PLAYER
NPC
VEHICLE
WEAPON
DOOR
BUILDING
ROAD
MISSION_OBJECT
```

Renderer maps semantic meaning to visual representation.

---

# 8. NO ASCII LOGIC IN CORE

Forbidden:

```text
if vehicle:
    character = 'V'
```

inside gameplay.

Correct:

```text
type = VEHICLE
```

ASCII renderer chooses glyph.

---

# 9. CAMERA

Camera state belongs to presentation layer where possible.

Simulation may expose controlled player transform/aim.

Renderer derives camera.

---

# 10. INTERPOLATION

Simulation runs fixed-step.

Renderer may interpolate between simulation snapshots for smoother output.

Interpolation must never feed back into game logic.

---

# 11. ASCII RENDERER GOAL

Priorities:

1. gameplay readability;
2. object recognition;
3. spatial comprehension;
4. decorative detail.

---

# 12. ANSI

ASCII frontend may use ANSI color/terminal controls.

Platform terminal handling remains outside simulation.

---

# 13. FRAMEBUFFER

Recommended internal representation:

```cpp
struct AsciiCell
{
    char32_t glyph;
    Color foreground;
    Color background;
    float depth;
};
```

Exact character type depends on terminal support.

---

# 14. DEPTH

3D ASCII renderer should maintain depth information to resolve visibility.

A z-buffer-like approach is acceptable.

---

# 15. PROJECTION

Renderer defines:

```text
camera transform
perspective projection
screen/terminal dimensions
```

Core simulation knows nothing about terminal columns.

---

# 16. WORLD GEOMETRY

Renderer may receive/generated presentation geometry for:

```text
roads
buildings
vehicles
characters
```

Static world geometry may be cached presentation-side.

---

# 17. CULLING

Renderer may perform:

```text
frustum culling
distance culling
occlusion approximations
```

These do not affect simulation visibility/perception rules.

---

# 18. SIMULATION PERCEPTION VS RENDER VISIBILITY

Important distinction:

```text
AI line-of-sight
```

belongs to simulation.

```text
whether terminal draws an object
```

belongs to renderer.

Never reuse render culling as gameplay perception.

---

# 19. HUD

HUD consumes a dedicated snapshot.

Canonical responsibilities include:

```text
health
weapon
ammo
wanted
witness count
missions
dialogue
map
Public Suspicion
Respect
faction info
territory changes
notifications
```

---

# 20. HUD INDEPENDENCE

HUD semantics should survive renderer replacement.

Do not hardwire HUD logic into terminal glyph positions inside gameplay.

---

# 21. HUD SNAPSHOT

Conceptually:

```cpp
struct HudSnapshot
{
    PlayerHudState player;
    WantedHudState wanted;
    MissionHudState mission;
    FactionHudState faction;
    NotificationList notifications;
};
```

---

# 22. INPUT IS NOT RENDERER

Renderer must not own gameplay input translation.

Platform/InputAdapter does.

---

# 23. RESOLUTION

ASCII renderer adapts to terminal size.

Gameplay camera framing may adjust presentation.

Simulation ranges remain unchanged.

---

# 24. DEBUG OVERLAYS

Development renderer should support:

```text
entity IDs
cell boundaries
streaming level
territory boundaries
AI state
faction presence
collision/debug geometry
```

---

# 25. RENDER SNAPSHOT COST

Snapshot extraction should avoid deep copy of entire world.

Only presentation-relevant nearby state.

Potential strategies:

```text
compact vectors
double-buffered snapshots
view extraction
```

Profile later.

---

# 26. THREADING

Renderer may eventually run separately.

Authoritative simulation must not be mutated by renderer thread.

Use immutable/double-buffered snapshots.

---

# 27. STATIC GEOMETRY CACHE

Generated buildings/roads may be cached by region.

Cache is derived and rebuildable.

---

# 28. ANIMATION

Animation state is presentation data where it does not affect gameplay.

Gameplay actions expose semantic state:

```text
walking
shooting
reloading
```

Renderer chooses exact animation/glyph timing.

---

# 29. EFFECTS

Visual effects:

```text
muzzle flash
damage flash
smoke approximation
notifications
```

must not be gameplay authority.

---

# 30. AUDIO

Future audio belongs beside presentation/platform, not simulation logic.

Simulation may emit semantic cues/events.

---

# 31. TERMINAL FAILURE

If terminal renderer cannot initialize, headless simulation should still work.

This is an architectural success criterion.

---

# 32. FUTURE GRAPHICAL RENDERER

A future renderer should be able to consume:

```text
semantic entities
world transforms
geometry references
HUD state
```

without changing systems such as:

```text
FactionSystem
MissionSystem
MarketSystem
PoliceSystem
```

---

# 33. TEST RENDERER

Implement optional:

```text
NullRenderer
```

or simply run without renderer.

Useful for headless tests.

---

# 34. SNAPSHOT VALIDATION

Ensure:

```text
no invalid entity references
finite transforms
supported semantic types
valid camera
```

---

# 35. FIRST MILESTONE

Implement:

```text
IRenderer
RenderSnapshot
CameraView
RenderObject
NullRenderer
```

---

# 36. SECOND MILESTONE

ASCII:

```text
terminal initialization
framebuffer
projection
depth
basic glyph mapping
```

---

# 37. THIRD MILESTONE

Add:

```text
roads
buildings
NPCs
vehicles
HUD
debug overlays
```

---

# 38. FOURTH MILESTONE

Optimize:

```text
region cache
interpolation
terminal diff updates
culling
```

only after profiling.

---

# 39. INVARIANTS

**RD1** — Renderer cannot mutate authoritative game state.

**RD2** — Core contains no ASCII-specific gameplay logic.

**RD3** — AI perception does not depend on render visibility.

**RD4** — HUD consumes semantic presentation data.

**RD5** — Renderer can be replaced without redesigning simulation.

**RD6** — Simulation can run with no renderer.

**RD7** — Presentation caches are derived.

---

# 40. STATUS

Final pre-code specification:

```text
14_V1_IMPLEMENTATION_ROADMAP.md
```
