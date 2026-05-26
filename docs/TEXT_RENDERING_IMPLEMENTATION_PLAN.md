# Text Rendering Implementation Plan

**Status:** execution plan  
**Design source:** `docs/TEXT_RENDERING_SERVICE.md`  
**Goal:** add Vulkan-rendered text overlays for render views without coupling
simulation/math code to font or Vulkan details.

## Phase 0: Service Contract

Purpose:

```text
Create a renderer-neutral text command service owned by EngineServices.
```

Status:

```text
complete
```

Files:

```text
src/engine/text/TextOverlayService.hpp
src/engine/text/TextOverlayService.cpp
tests/test_text_overlay_service.cpp
```

Behavior:

```text
Submit frame-local text commands.
Store target RenderViewId, coordinate space, anchor, position, color, size, font role, and text.
Filter commands by view.
Clear commands per frame.
Track configurable default font path.
```

Completion gate:

```text
EngineServices exposes TextOverlayService and tests pass.
```

---

## Phase 1: Font Asset Configuration

Purpose:

```text
Make the bundled TTF/OTF font discoverable, loadable, and cached through
ResourceManagerService.
```

Status:

```text
complete
```

Initial default:

```text
assets/fonts/static/STIXTwoText-Regular.ttf
```

Desired later default:

```text
assets/fonts/<monospaced-font>.ttf or .otf
```

Completion gate:

```text
ResourceManagerService owns a FontResource payload with cached bytes.
TextOverlayService stores the active font ResourceHandle / ResourceId.
Runtime config can choose a TTF/OTF font path and the service reports it.
```

Implementation notes:

```text
Do not let TextRenderer read files directly.
TextRenderer must consume FontResource::bytes through TextOverlayService.
Font resources use ResourceKind::Font and ResourceOwner::Renderer.
```

---

## Phase 2: Renderer Text Atlas

Purpose:

```text
Load the configured TTF/OTF font and build a GPU font atlas.
```

Files:

```text
src/renderer/TextRenderer.hpp
src/renderer/TextRenderer.cpp
```

Implementation notes:

```text
Use vendored FreeType as the font rasterizer.
Create FreeType library/face objects from ResourceManager-owned FontResource bytes.
Own Vulkan image/sampler/descriptors with RAII.
Start with ASCII printable glyphs plus common math punctuation.
Rebuild atlas on font change or device recreation.
Keep glyphs as renderer-owned derived cache entries, not ResourceManager records.
```

Completion gate:

```text
Renderer can create and destroy a font atlas without leaks.
```

---

## Phase 2A: FreeType Vendor Boundary

Purpose:

```text
Make FreeType available to the text renderer while keeping font file loading in
ResourceManagerService.
```

Status:

```text
in progress
```

Build rule:

```text
FreeType is vendored with FetchContent and linked through ndde::freetype.
Optional FreeType compression/image dependencies are disabled for the first
slice: zlib, bzip2, brotli, png, and harfbuzz.
```

Ownership rule:

```text
ResourceManagerService owns FontResource bytes.
TextOverlayService owns text draw commands and active font handles.
TextRenderer owns FreeType faces, glyph metrics, atlas pages, and Vulkan objects.
Individual glyphs are cache records, not public ResourceId entries.
```

Completion gate:

```text
Debug and tidy builds configure and link with FreeType available to ndde_engine.
```

---

## Phase 3: Text Quad Generation

Purpose:

```text
Convert TextDrawCommand objects into glyph quads.
```

Behavior:

```text
ScreenPixels and NormalizedViewport map to view pixels.
Domain coordinates map through RenderViewDomain.
Anchor controls local origin.
Commands generate compact vertex/index batches.
```

Completion gate:

```text
Unit tests validate text bounds, anchors, and coordinate mapping.
```

---

## Phase 4: Vulkan Text Pipeline

Purpose:

```text
Render text batches over geometry in main and alternate views.
```

Implementation notes:

```text
Alpha blending enabled.
Depth disabled or late overlay pass.
Separate text shader pair or reusable textured-quad pipeline.
Flush after regular render packets.
Handle swapchain recreation.
```

Completion gate:

```text
Labels are visible in both windows and screenshots.
```

---

## Phase 5: Integration Lab Labels

Purpose:

```text
Use TextOverlayService in the Integration Lab analytics view.
```

Initial labels:

```text
Convergence
log(error)
grid resolution
Midpoint
Trapezoid
estimate/error summary
selected cell id and contribution
```

Completion gate:

```text
The second-window analytics view is readable without relying on ImGui panels.
```

---

## Do Not Do Yet

```text
Do not build a full layout/rich text engine.
Do not put text on the event bus.
Do not make simulations own fonts.
Do not make each glyph a ResourceManager resource.
Do not add LaTeX rendering before basic labels work.
Do not optimize with SDF/MSDF until bitmap atlas text is proven insufficient.
```
