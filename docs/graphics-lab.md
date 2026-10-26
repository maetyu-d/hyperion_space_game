# Graphics Lab / promoted in Hyperion 3.56

The upgraded renderer is now the default High graphics mode in the full game.
Settings offers Low to restore the previous rendering settings and lighting.
`Launch Graphics Lab.command` opens the installed game with High graphics and
disables save writes for testing. Normal launches retain campaign saving.

The experiment adds layered nebular gas and foreground dust around the spectral
silk, twilight-aware sky lighting and water reflections, wavelength-dependent
distance haze, softer diffuse material bounce, cloud powder scattering and silver
edges, and translucent lighting for alien life. A bounded clarity pass reuses the
existing antialiasing samples and avoids sharpening silhouettes and bright points.

Procedural geography, collision surfaces, gameplay and save formats are unchanged.
Full effects raise the scene ceiling from 1080p to 1440p, clouds from one-third
to half resolution with 24 rather than 16 integration steps, local detail and
sun shadows from 1024 to 2048, solid-planet surface fields from 2048 to 3072,
and geology fields from 1536 to 2048. Reduced effects retain the 810p ceiling,
one-third cloud resolution and ten steps. Reduced effects uses the selected High/Low mode’s material and shadow maps.
This trades GPU time and memory for image quality.
The nebula reuses existing noise samples.

## Smoother life streaming

Flight now queries a 360-unit chart radius rather than 210, with a 1024-candidate
ceiling rather than truncating to the nearest 150. Candidates fade from 180 to
330 chart units, before the generation boundary. Actual drawing fades between
180 and 340 world units. This also fades life before the altitude gate closes.

Full effects blend detailed and simple anatomy over 28–64 world units. Fixed,
complementary coverage masks preserve opaque depth without overlapping transparent
bodies or animated noise. Near-camera feet no longer cause an entire body to vanish.
Dense views retain a draw budget of 256 bodies (128 in reduced effects); the budget
edge fades, with its strength increasing gradually as the view fills. Low graphics
uses the previous 150-body budget. Coverage-weighted priority and a short,
quarter-second presentation filter stabilize dense views as cohorts enter or
leave the camera. This filter uses visual time and never advances the simulation. Contact
shadows share body coverage and fade between 65 and 105 world units.

The explicit native regression target `hyperion_life_continuity_check` flies a
480-frame approach and departure, checks the tracked cohort's coverage and detail
continuity, and verifies the economy and clock stay frozen. Build it explicitly
with CMake and pass a capture folder when running it; it is excluded from normal
builds and from the windowless CPU test suite.

Native captures and frame-time reports are kept in `captures/graphics-upgrade/`.
They use fresh, no-save scenarios. Flight benchmark runs use 60 warm-up frames and
180 measured frames without vsync. Compare reports only at matching output sizes
and settings; the macOS Retina output may be larger than the requested window.

## Measured quality tradeoff

One sequential sample on this Mac, with a 960 × 600 window and 1920 × 1200
Retina output. Full effects were enabled. The old renderer internally capped
its scene at 1080p; the experiment rendered this output at 1200p.

| Scene | Installed 3.55 | Graphics Lab |
| --- | ---: | ---: |
| Ringed planet / orbit | 49.9 FPS / 20.06 ms | 35.3 FPS / 28.34 ms |
| Near-surface life | 28.6 FPS / 35.02 ms | 30.2 FPS / 33.10 ms |

These are short scene samples, not a minimum-FPS guarantee. GPU clocks,
thermal state and scene content affect results. The orbital cost is substantial;
the surface difference is small enough to treat as run-to-run variation.

Life-continuity follow-up: all 34 CPU suites and the 480-frame approach check passed.
The reduced-effects check captured all eight scenes. A matched full-quality surface
sample measured 29.3 FPS before and 23.7 FPS after, with the rendered population
increasing from 91 to 253 bodies. The broader candidate radius also adds CPU work.
These remain short samples; the original installed release is preserved.
