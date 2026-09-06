# Video shaders

Salvia's video post-processing shaders are **external data, not code**. They live in
`assets\shaders` and are discovered and compiled at runtime, so adding, editing or
removing one needs **no rebuild** — just drop the files in and restart the frontend.

This document describes the file format, every parameter it accepts, and the contract a
shader body must satisfy.

## Format at a glance

Each shader is a pair of files:

| File | Role |
|---|---|
| `<name>.hlslp` | **Preset**: a RetroArch-style `key = value` text file describing sampler state, LUTs and the menu label. This is what makes the shader appear in Salvia. |
| `<name>.hlsl` | **Body**: the actual pixel shader, HLSL `ps_3_0`. Optional — a preset with no body only changes sampler state. |

> **Why `.hlslp`/`.hlsl` and not `.slangp`/`.slang`?** The preset *syntax* is RetroArch's,
> but the body is **HLSL**, not GLSL. A RetroArch `.slang` is Vulkan GLSL and cannot be fed
> to `D3DXCompileShader`, so the extensions were changed to avoid implying compatibility
> that does not exist. Porting a `.slang` shader means rewriting its body in HLSL.

## Discovery and ordering

* The directory scanned is `<appDir>\assets\shaders`. Only `*.hlslp` files are considered;
  `.hlsl` bodies are pulled in by the preset that references them.
* The **preset id** is the filename without the `.hlslp` extension (`05-crt-geom`). It is
  what gets written to the config files, so it must not be empty, must be under 64
  characters, and must not contain `=`, `#`, `\`, `/` or line breaks.
* Presets are listed **in alphabetical order**, which is also the order shown in the menu.
  That is the only reason the shipped presets carry a numeric prefix (`00-`, `01-`, …):
  it pins their position. Your own shader can be named anything.
* Up to **64 presets** are loaded. Duplicated ids and unparseable presets are skipped with
  a message in the log.
* If *no* valid preset is found, Salvia synthesises a Nearest and a Bilinear entry so the
  menu is never empty.

## Preset file (`.hlslp`) reference

Syntax rules:

* One `key = value` per line. Keys are **case-insensitive**; a repeated key wins over the
  earlier one.
* Surrounding double quotes are stripped, so `filter_linear0 = "false"` and
  `filter_linear0 = false` are the same.
* Lines starting with `#`, `;` or `[` are ignored.
* **Inline comments are not supported.** Everything after the `=` is the value, so
  `wrap_mode0 = "repeat"   # nope` silently becomes garbage (and falls back to the
  default). Put comments on their own line.
* Paths are relative to the preset's own directory. Both `/` and `\` work — they are
  normalised to `\` internally, because the Xbox 360 XDK does *not* translate `/` for you.

### Pass keys

The trailing `0` is the pass index. Multi-pass is reserved in the format but **not
implemented**: only pass `0` is ever executed.

| Key | Default | Meaning |
|---|---|---|
| `shaders` | `1` | Number of passes declared. `< 1` rejects the preset. `> 1` is accepted and logged, but only pass 0 runs. |
| `shader0` | *(none)* | Body file to compile. **If omitted, the built-in passthrough is used** — that is how a "shader" that only changes filtering (Nearest, Bilinear) is expressed. If given but unreadable, the whole preset is rejected. |
| `filter_linear0` | `false` | Sampler `s0` min/mag filter: `true` → linear, `false` → point. Accepts `true/1/yes/on` and `false/0/no/off`. |
| `wrap_mode0` | `clamp_to_edge` | Sampler `s0` addressing: `clamp_to_edge`, `repeat`, `mirrored_repeat` (or `mirror`). Anything unrecognised falls back to clamp. |
| `salvia_precision0` | `half` | `half` compiles with partial precision (16-bit), which doubles the effective temp registers and is enough for most 2D filters. `full` forces 32-bit floats — needed when the maths involves large values (e.g. `fmod(vpos.x, 3)` at 1080p+ produces visible vertical banding in half). |

### LUT (lookup texture) keys

These are **global to the preset**, not per pass. Up to **3 LUTs**; extras are ignored.

| Key | Default | Meaning |
|---|---|---|
| `textures` | *(none)* | Names of the LUTs to load, separated by `;`, `,`, spaces or tabs. Each name is just a label used to look up the keys below. |
| `<NAME>` | — | PNG file for that LUT, relative to the preset. Required if the name is listed in `textures`. |
| `<NAME>_linear` | `false` | Filtering for that LUT's sampler. |
| `<NAME>_wrap_mode` | `clamp_to_edge` | Addressing for that LUT's sampler. |
| `salvia_sampler_<NAME>` | declaration order | Destination sampler register, `1`–`3`. By default the first LUT goes to `s1`, the second to `s2`, and so on. Out-of-range values revert to the default. |

The PNG is decoded with SDL_image and uploaded as `A8R8G8B8`. It is treated as **data, not
an image**: no alpha premultiplication or surface conversion is applied, so LUTs with
near-zero alpha survive intact.

### Menu keys

| Key | Meaning |
|---|---|
| `salvia_label` | Name shown in the menu. |
| `salvia_label_key` | Optional i18n key. If the language manager resolves it, it wins over `salvia_label`; otherwise `salvia_label` is used. Translations live in the external language `.ini` files, not in the repo. |

If neither key is present the id is prettified instead (`05-crt-geom` → `crt geom`).

## Shader body (`.hlsl`) contract

* Profile **`ps_3_0`**, entry point **`main`**, output semantic **`COLOR0`**.
* There is **no vertex shader to supply**: the quad is drawn with pretransformed vertices
  through the fixed-function vertex stage. You only write the pixel shader.
* The pass runs **once per frame, straight to the backbuffer**, clipped by a scissor
  rectangle so letterboxing/pillarboxing is handled outside the shader.

### Inputs

```hlsl
struct PS_IN { float2 TexCoord : TEXCOORD0; };   // UV in [0,1] over the game texture
float4 main(PS_IN In) : COLOR0 { ... }
```

`VPOS` is also available if you need screen-space pixel coordinates (the CRT shaders use it
for the aperture mask):

```hlsl
float4 main(PS_IN In, float2 vpos : VPOS) : COLOR0 { ... }
```

### Registers provided by the engine

| Register | Contents |
|---|---|
| `s0` | The game texture, at the **core's native resolution**. |
| `s1`–`s3` | LUTs declared through `textures`, in the order given (or as forced by `salvia_sampler_<NAME>`). |
| `c1.xy` | `textureDims` — width and height of the game texture in pixels. `.zw` are unused. Written on every effect change, so it is always valid. |
| `c0.x` | Legacy filter-type selector. Currently always `0`; kept for compatibility with the original embedded passthrough. Treat it as reserved. |

Everything else is **not** provided: there is no output/viewport size, no frame counter, no
projection matrix, no access to previous frames, and no intermediate render targets. There
is also **no pre-upscale pass** — the texture is always at core resolution, so a
scale-aware algorithm must derive its subpixel position from `textureDims` itself
(`frac(uv * textureDims)` is the usual idiom).

The alpha channel of the returned colour is **ignored**: the game quad is drawn with alpha
blending disabled.

### Minimal template

`assets\shaders\passthrough.hlsl` is shipped as a starting point. The smallest useful
shader is:

```hlsl
float2 textureDims : register(c1);
sampler2D detail : register(s0);
struct PS_IN { float2 TexCoord : TEXCOORD0; };

float4 main(PS_IN In) : COLOR0 {
    float2 texel = 1.0 / textureDims;
    return tex2D(detail, In.TexCoord);
}
```

with a preset next to it:

```ini
shaders = "1"
shader0 = "my-filter.hlsl"
filter_linear0 = "false"
wrap_mode0 = "clamp_to_edge"
salvia_precision0 = "half"

salvia_label = "My filter"
```

### Adding a LUT

```ini
shaders = "1"
shader0 = "my-filter.hlsl"

textures = "LUT"
LUT = "my-table.png"
LUT_linear = "false"
LUT_wrap_mode = "clamp_to_edge"
```

```hlsl
sampler2D detail : register(s0);
sampler2D lutTex : register(s1);
```

## Selecting a shader

* **Menu**: Video options, shader entry.
* **Globally**: `shaderMode = <preset id>` in the main `.cfg` (the id, without the
  `.hlslp` extension). A missing preset falls back to the default.
* **Per emulator**: the same `shaderMode` key in the emulator's `.cfg` overrides the global
  one. Leave it empty to inherit the global setting.
* Numeric values `0`–`12` written by older versions are migrated automatically to the
  matching shipped preset id.

## Things to watch out for

* **A shader that fails to compile is not fatal**: that preset silently falls back to the
  built-in passthrough. On Xenon there is no fixed-function pipeline, so binding a NULL
  pixel shader would be a black screen — the fallback always exists. Check the log if a
  filter appears to do nothing.
* **Xbox 360 compile cache**: compiled bytecode is cached in `game:\shadercache\` under a
  hash of the source *and* the compile flags. Editing a `.hlsl` (or flipping
  `salvia_precision0`) changes the hash, so it recompiles by itself — no need to clear the
  cache by hand.
* **Both backends share the shaders.** The Xbox 360 path
  (`libs\libSDLx360\...\SDL_xboxvideo.c`) and the Windows path (`src\video\win_d3d9.cpp`)
  compile the same HLSL with the same flags, so a shader written for one works on the
  other. Anything that behaves differently is a bug worth reporting.
* Keep the maths cheap. On the console the pass runs at backbuffer resolution every frame;
  a heavy shader shows up straight away as dropped frames.
