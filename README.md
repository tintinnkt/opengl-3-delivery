# Cake Delivery Run

A simple 3D top-down delivery game built with OpenGL as a university graphics assignment.

## Idea

Run around a walled arena, pick up cakes, and drop them off at plate markers — all before the 60-second timer runs out. Three cakes, three plates, one player.

## Controls

| Key   | Action        |
| ----- | ------------- |
| `W`   | Move forward  |
| `S`   | Move backward |
| `A`   | Move left     |
| `D`   | Move right    |
| `ESC` | Quit          |

Proximity handles everything else — walk close enough to a cake to pick it up, walk close enough to a plate to deliver it.

## Technical stuff

**Rendering**

- OpenGL 3.3 Core Profile via GLFW + GLAD
- Single Phong-style shader with a `useColor` flag to switch between flat-color geometry and textured `.obj` models
- Procedural geometry for the ground (quad) and walls/markers (unit cube), loaded models for player, buildings, cake, plate

**Models**

- Loaded with `learnopengl/model.h` (Assimp under the hood)
- Player: `character-male-b.obj`
- Inner obstacles: `building-a.obj`
- Pickups: `cake.obj`
- Delivery zones: `plate.obj`

**Camera**

- Fixed isometric-style follow cam — constant offset `(0, 10, -10)` above and behind the player
- Rebuilds `view` matrix every frame with `glm::lookAt`

**Collision**

- Player vs walls: AABB overlap test, push-out on the axis of least penetration
- Player vs cakes: 2D circle distance check (XZ plane only)
- Player vs plates: same circle check, triggers delivery if carrying a cake

**Game loop**

- `deltaTime` based movement and timer
- State flags: `carryingPackage`, `gameOver`, `gameWon`
- Progress printed to stdout
