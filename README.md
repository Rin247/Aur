# Aur — 2D-HD Desktop RPG Engine & Game Studio

Aur is an advanced, AGPL-3.0 2D-HD RPG game engine and game creation studio built with C++20 and Qt 6 / QML (`Aur.exe`). Combining a 2.5D/HD-2D pixel-art runtime engine (**Vulkan 1.3 & DirectX 12_1**) with integrated map editors, sprite animators, tileset cutters, and a built-in DAW music editor, Aur gives indie developers a complete suite to design and publish retro-inspired RPGs.

---

## ⚡ Core Subsystems & Status Matrix

| Subsystem | Features & Capabilities | Status / Roadmap |
|---|---|---|
| **Graphics Engine** | **Vulkan 1.3 & DirectX 12_1** 2D-HD rendering engine | Active |
| **RPG Runtime** | Tilemap collision, party movement, turn-based battle engine, dialog system | Active |
| **HD-2D Rendering** | Dynamic 3D tilt/DoF post-processing (Octopath-style), 2D normal map lighting, weather particles | Roadmap |
| **Map & World Editor** | Multi-layer tilemap painter, collision layer editor, event placer, NPC pathing | Active |
| **Procedural World** | Procedural dungeon generator, auto-tiling ruleset engine, real-time light placer | Roadmap |
| **Sprite Animator** | Frame-by-frame sprite animator, hitbox/hurtbox editor, palette swapper, grid cutter | Active / Expanding |
| **2D Skeletal Animation** | Bone-based 2D skeletal animation, sprite sheet auto-packer | Roadmap |
| **Chiptune & DAW** | Built-in synth tracker, SFX generator, MIDI pattern composition editor | Active / Expanding |
| **Audio Engine** | Interactive music state transition manager, VST3 plugin host, spatial audio emitters | Roadmap |

---

## 🗡️ Key Features & Studio Modules

### 🎮 Runtime RPG Engine (`engine`)
- **Current**: 2D-HD sprite & tile rendering engine, tilemap collision, party movement, turn-based battle state machine, and dialog box system.
- **Planned / Roadmap**: Dynamic 3D tilt/depth-of-field post-processing (Octopath-style HD-2D), dynamic lighting/shadows, and weather particle systems.

### 🗺️ World & Map Editor (`studio/map`)
- **Current**: Multi-layer tilemap editor, collision layer painter, event trigger placer, and NPC pathing tool.
- **Planned / Roadmap**: Procedural dungeon generator, auto-tiling ruleset engine, and real-time lighting editor.

### 🎨 Sprite & Tileset Animator (`studio/sprite`)
- **Current**: Frame-by-frame sprite animator, hitbox/hurtbox editor, palette swapper, and tileset grid cutter.
- **Planned / Roadmap**: Automatic normal map generator for 2D lighting, bone-based 2D skeletal animation, and sprite sheet packer.

### 🎵 Chiptune & Music DAW (`studio/daw`)
- **Current**: Built-in synth tracker, sound effect (SFX) generator, and MIDI pattern composition editor.
- **Planned / Roadmap**: Interactive music state transition manager, VST3 plugin host, and spatial audio emitter placement.

---

## 🏗️ Architecture & Technology Stack

- **Graphics APIs**: Vulkan 1.3 & DirectX 12_1.
- **UI & Studio Tools**: Qt 6.12, QML, Custom dark theme interface.
- **Game Runtime**: Pure C++20 core engine (`engine/`).
- **Licensing**: AGPL-3.0.

---

## 🛠️ Building & Running

### Prerequisites
- **Visual Studio 2022** (C++20 MSVC compiler)
- **Qt 6.12** (`msvc2022_64`)
- **Vulkan SDK 1.3+**

### Build Commands

```bat
:: Build Release binary (Aur.exe)
build.bat Release

:: Launch Aur
run.bat
```

---

## 📜 License

Aur is released under the **GNU Affero General Public License v3.0** (AGPL-3.0). See [`LICENSE`](LICENSE) for details.
