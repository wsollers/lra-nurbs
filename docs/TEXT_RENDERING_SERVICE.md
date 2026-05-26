# Text Rendering Service Design

**Status:** design contract  
**Scope:** Vulkan-rendered labels, plot text, legends, axis ticks, and view overlays  
**Owner:** `EngineServices` for commands, renderer layer for glyph atlas and Vulkan drawing  
**Intent:** make non-ImGui render views readable without coupling simulations to font rendering

## Purpose

The Integration Lab, Taylor Lab, spline/NURBS lab, metrics view, and diagnostic
views need text inside Vulkan-rendered views.

ImGui remains the right tool for panels and controls. It is not enough for:

- second-window analytics labels
- axis ticks and plot labels
- selected-cell annotations
- heatmap legends
- method-comparison labels
- future curve/surface/NURBS labels
- capture-ready overlays in render views

`TextOverlayService` is the engine-facing command surface. It receives compact
text draw requests and stores frame-local text commands. The renderer consumes
those commands through a later `TextRenderer` glyph-atlas implementation.

```text
Simulation / Lab code
  -> TextOverlayService text commands
  -> Renderer TextRenderer batches glyph quads
  -> Vulkan pipeline draws text in the target view/window
```

## Resource Interchange

Font files are resources, not ad hoc filesystem reads.

The ownership boundary is:

```text
ResourceManagerService
  registers font path resources
  loads TTF/OTF files
  caches font bytes in FontResource payloads
  owns ResourceHandle -> current ResourceId mapping

TextOverlayService
  stores the active font ResourceHandle / ResourceId
  exposes active FontResource metadata and bytes to renderer code
  does not read font files directly

TextRenderer
  receives FontResource bytes from ResourceManagerService through
  TextOverlayService
  builds/rebuilds FreeType-backed glyph caches and Vulkan font atlases from
  cached bytes
```

Expected flow:

```text
Engine startup or lab setup
  -> TextOverlayService::register_and_load_font(key, path)
  -> ResourceManagerService::register_path(...)
  -> ResourceManagerService::load(...)
  -> ResourcePayload = FontResource{path, bytes, byte_count}
  -> TextOverlayService binds active font handle/id
  -> TextRenderer builds atlas from FontResource::bytes
```

This keeps font identity, lifetime, and cached bytes consistent with the rest of
the engine resource system.

Glyphs are derived renderer data, not individually registered public resources.
`ResourceManagerService` owns the font-file resource. `TextRenderer` owns
`FontResourceId + pixel size + font role + atlas generation` caches containing:

- FreeType face/size-derived glyph metrics
- glyph bitmap placement in an atlas page
- advance, bearing, and quad layout data
- Vulkan image/sampler/descriptor state for atlas pages

This avoids creating hundreds of tiny `ResourceId` records for individual
glyphs while still keeping font loading and lifetime auditable through the
resource manager.

## C++ Engineering Standard

Implementation follows the project standard: modern C++20/23, clear ownership,
Rule of Zero for value types, project scalar/id types, and narrow dependencies.
Use `std::optional`, `std::expected`, `std::span`, and PMR containers where they
reduce lifetime ambiguity.

Text commands are UI/render presentation data. They are not compact simulation
events and must not be placed on the event bus. Simulation code should publish
ids, numeric values, and metadata; render/UI layers format human-readable text.

## Font Asset

The service should load fonts from `assets/fonts`.

Current bundled font candidates:

```text
assets/fonts/STIXTwoMath-Regular.ttf
assets/fonts/STIXTwoMath-Regular.otf
assets/fonts/static/STIXTwoText-Regular.ttf
```

The desired default is a monospaced TTF/OTF font when one is present in assets.
The service keeps the font path configurable so the renderer can switch to a
true mono font without changing calling code.

Initial label work should use small point sizes, roughly 12 to 16 screen pixels
on the target view. A label such as `"TTTT"` at a selected point should be a
normal text draw request, not custom mesh geometry.

## Font Rasterizer

The renderer-side implementation uses FreeType.

Rationale:

- supports both TTF and OTF assets already expected by the project
- preserves access to real glyph metrics, bearings, kerning-ready advances, and
  future shaping hooks
- avoids hand-authored bitmap alphabets in analysis views
- keeps simulation and math code independent from font details

FreeType is vendored through CMake `FetchContent` and linked into the engine
layer for the text service implementation. The first atlas milestone uses
grayscale bitmap glyphs with alpha blending. Signed-distance-field text and
complex shaping are later refinements.

## Service Split

Use two layers:

```text
TextOverlayService
  engine-owned, frame-local command collection
  no Vulkan calls
  validates view ids and command bounds

TextRenderer
  renderer-owned Vulkan implementation
  FreeType face loading from ResourceManager cached font bytes
  font atlas / glyph cache built from FreeType glyph bitmaps
  glyph quad generation
  Vulkan pipeline, descriptor, sampler ownership
```

This keeps simulation and lab code independent from Vulkan details.

## Initial Command Model

```cpp
enum class TextCoordinateSpace : u8 {
    ScreenPixels,
    NormalizedViewport,
    Domain
};

enum class TextAnchor : u8 {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
};

struct TextDrawCommand {
    RenderViewId view;
    TextCoordinateSpace space;
    TextAnchor anchor;
    Vec2 position;
    Vec4 color;
    f32 size_px;
    TextFontRole font;
    std::pmr::string text;
};
```

Coordinate meanings:

```text
ScreenPixels:
  position is view-local pixels.

NormalizedViewport:
  position is [0,1] x [0,1], top-left UI convention.

Domain:
  position is in the current RenderViewDomain, useful for axis labels and
  selected-cell annotations.
```

## First Consumers

Integration Lab:

- convergence plot axis labels
- method comparison labels
- heatmap legend
- selected-cell annotation
- status strip in the second window

Metrics/diagnostics:

- plot labels
- timeline labels
- counter labels

Spline/NURBS lab:

- knot labels
- basis-function labels
- curvature/torsion graph labels
- frame labels

## Renderer Milestones

1. Build a CPU-side text command service with tests.
2. Add font asset discovery/config.
3. Add renderer-side FreeType font face creation from `FontResource::bytes`.
4. Add bitmap glyph atlas construction for the configured TTF/OTF.
5. Generate glyph quads into a text vertex format.
6. Add a Vulkan text pipeline with alpha blending.
7. Flush text commands after regular geometry packets for each view.
8. Add integration analytics labels.

## Non-Goals For The First Slice

```text
No rich text.
No paragraph layout.
No LaTeX rendering.
No signed-distance-field text until basic bitmap atlas text works.
No event-bus text payloads.
No simulation ownership of font resources.
```

## Acceptance Criteria

The first useful service slice is acceptable when:

```text
EngineServices owns TextOverlayService.
Labs can submit text commands for a view.
Commands support screen, normalized, and domain coordinate spaces.
The default font path points into assets/fonts.
Commands are frame-clearable.
Tests verify command storage, filtering, font config, and frame clear.
```

The first Vulkan-rendered text slice is acceptable when:

```text
The second window displays labels from TextOverlayService.
Text appears in captures.
Alpha blending and color work.
Labels survive resize/recreate.
No text work runs on simulation/event hot paths.
```
