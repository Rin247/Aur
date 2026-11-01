# Aur MCP Server

Aur includes a local stdio MCP server for project-aware design tasks. Build with `.\build.bat`, then register `build\Release\aur_mcp.exe` with an MCP client and pass the project directory as its only argument:

```json
{
  "mcpServers": {
    "aur": {
      "command": "C:\\Projects\\Aur\\build\\Release\\aur_mcp.exe",
      "args": ["C:\\Projects\\Aur\\projects\\sylvi"]
    }
  }
}
```

The server exposes tools to inspect projects, list/read maps, graphics, and biomes; paint terrain or biome layers; create and update biome definitions; and generate maps and assets. Seeded generators create eight-direction NPCs (age groups `child`, `teen`, `adult`, `elder`), creatures and cat/dog pets, animated effects, and indexed terrain tiles. Character generation optionally accepts a `gender` identity (`female`, `male`, `non-binary`, `unspecified`) and a distinct visual `presentation` (`feminine`, `masculine`, `androgynous`); generated character sprites do not include weapons. `world_generate` creates a separate equirectangular Earth-like world map. Its `water_level`, `vegetation`, and `decay` settings are integers from 0 to 100; optional `axial_tilt` is 0–359 degrees and rotates the equator/pole axis. These settings are saved with the seed in the map's `worldgen` object, and the map includes a per-tile temperature layer. Mutating tools validate inputs and save changes to the supplied project directory immediately. The server uses stdio only; it does not open a network listener or accept arbitrary file paths.

The 1280×720 game output does not constrain map or authored graphic dimensions. Graphics and tiles currently use the engine's indexed 16-color format; MCP edits validate against that serialization. The MCP server rejects a single graphic above 16 megapixels of total frame data or 4096 pixels on either side to avoid excessive allocations. Palette expansion is a data-format evolution, not a fixed visual-design requirement.

Project biome definitions live in `biomes.json`; maps store painted biome IDs in `layers.biomes`. Default presets cover grasslands, forests/jungle, desert, alpine/tundra, marsh, volcanic badlands, coasts, ruins, crystal reaches, and sky isles. A biome references a tileset/base tile and carries an editor overlay color plus vegetation, decay, and water-level metadata. The World editor keeps existing maps intact when generating a new world and requires a fresh map ID.

Sylvi ships with `maps/earthlike_world.json`, a separate 256×128 generated map using seed `20250308` and defaults of 50% water, 65% vegetation, and 15% decay. Its markers and biome painting are available immediately, while the existing Aurelia map and project start remain unchanged.

All generation tools accept explicit integer seeds so outputs can be reproduced and iterated. The Studio Asset Forge additionally draws editable clothing and props as indexed pixel art; weapon assets are created separately from character sprites. Generated sprites/effects remain editable through the Sprite and Pixel editors. Pixel Editor auto-fit treats 1280×720 as its minimum working canvas and scales up with available space; manual zoom is available through 64×.

After an agent changes files, use Studio's **Reload Agent Changes** button to refresh the open project. Studio asks for confirmation because reloading discards unsaved in-editor edits.

The Studio UX baseline now includes explicit project open/save, dirty-project protection, and bounded undo/redo for map paint strokes and pixel-frame edits; Pixel/Sprite Editors have same-size frame clipboard actions. This history is canvas-scoped, not yet a transactional undo stack for arbitrary project/MCP edits.
