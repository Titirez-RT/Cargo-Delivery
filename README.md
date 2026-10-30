# Cargo Delivery - 3D OpenGL Minigame 🚂📦

## About The Project
**Cargo Delivery** is a custom-built 3D minigame developed in C++ and OpenGL. The project simulates a time-sensitive railway logistics system where a train automatically navigates a complex, procedurally generated 3D rail network. The player's goal is to route the train at junctions to collect specific resources scattered across different stations and return them to the Central Station within a 60-second time limit.

## Key Features
*   **Graph-Based Rail Network:** The rail system is modeled as a directed graph. Each rail segment is a node containing start/end coordinates and vectors of possible connection paths (neighbors).
*   **Hierarchical Transformations & Kinematics:** The train's movement uses Linear Interpolation (LERP) across the rails. The locomotive calculates its rotation dynamically using the arctangent (`atan2`) of the rail's coordinates.
*   **Perfect Wagon Trailing:** The attached wagon follows the locomotive's exact path flawlessly, even through complex multi-branch junctions, utilizing an `std::queue` data structure to track and consume rail indices.
*   **GPU-Accelerated Time Limit:** Instead of traditional UI timers, the game passes the remaining time as a uniform to the Fragment Shader. The Central Station dynamically shifts its color from gray to intense red on the GPU as time expires.
*   **Custom Minimap:** A secondary orthographic camera renders a real-time minimap in a dedicated viewport, clearing the depth buffer to overlay onto the main screen.

## Controls
*   **[A] / [D]**: Choose the next rail direction when the train reaches a junction.
*   **[V]**: Toggle the camera perspective (Free Camera / Train Follow Camera).
*   **[R]**: Restart the game (available on the End Game screen).

## Technical Implementation Highlights
1.  **The Map Generation:** Building a highly complex 3D rail system with over 60+ intersecting coordinate sets is challenging. The map was initially planned and color-coded in MS Paint to create a visual node map before hardcoding the precise 3D vertices into C++.
2.  **The `std::queue` Wagon System:** To prevent visual stuttering or corner-cutting when the locomotive changes direction, the locomotive pushes every entered rail index into a queue. The wagon consumes this queue step-by-step, maintaining a mathematically calculated gap (offset) to ensure a perfectly smooth trailing animation.
