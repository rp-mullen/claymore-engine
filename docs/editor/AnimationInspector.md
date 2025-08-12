# Animation Inspector

This editor panel previews `.anim` clips in an isolated, offscreen viewport using bgfx. It does not affect the main scene or play mode.

Highlights:
- Offscreen framebuffer with its own view id, camera controls (orbit/dolly/pan), and reset.
- Deterministic rebuild when the selected clip changes.
- Playback controls: play/pause, loop, speed, time/frame scrub.
- Humanoid retargeting hook via `AvatarDefinition` if the clip encodes `avatarPath`.
- Ticks only while visible; no input bleed outside hovered viewport.

Integration:
- The `Project` panel exposes the selected file path. When it ends with `.anim`, `UILayer` routes the Inspector window to `AnimationInspectorPanel` instead of the default entity inspector.

Key Classes:
- `AnimationInspectorPanel` – ImGui UI and orchestration.
- `PreviewScene` – offscreen bgfx FBO + camera & minimal render context.
- `PreviewAvatarCache` – resolves a source avatar (from `AnimationClip.SourceAvatarPath`) or a default mannequin.
- `AnimationPreviewPlayer` – advances time, evaluates the clip onto the preview skeleton.

Notes:
- Humanoid retargeting is stubbed to use `AvatarDefinition` as a mapping descriptor. Extend to apply `HumanoidRetargeter` for full fidelity.
- Asset hot-reload: wire to the existing `AssetWatcher` to call `LoadClip()` when the selected file changes and the toggle is enabled.

