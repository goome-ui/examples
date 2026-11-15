<h1 align="center">🧩 goome in a game engine 🧩</h1>

<p align="center">
<b>Small examples of goome running inside something that already has a window and a renderer.</b>
</p>

<p align="center">
When you're making your own game or using an engine, you don't inject anything. goome runs offscreen, the engine uploads its frame as a texture, and you feed the engine's input in. The renderer and platform backends in <a href="../vendor/backends">vendor/backends</a> do the work. Each folder is one engine with a short README and the exact build command.
</p>

| folder | engine | how |
|---|---|---|
| [raylib](raylib) | raylib 5.5 | offscreen goome + the `goome_gl3` renderer backend in raylib's GL context |
| [sdl_gl3](sdl_gl3) | SDL2 + OpenGL 3 | the `goome_sdl2` platform backend + `goome_gl3` renderer backend |
| [love2d](love2d) | LÖVE (Lua) | luajit ffi to a goome shared lib, frame copied into a love Image |

<p align="center">
All three block the game's input while goome wants it, the same as the native mods do. SDL3 works too, swap <code>goome_sdl2.h</code> for <code>goome_sdl3.h</code> and the <code>sdl2</code> in the names for <code>sdl3</code>. For anything with its own texture api and no goome backend, copy the frame yourself with <code>goome_frame_dirty</code> and <code>goome_frame_copy</code>, backends/README.md shows that.
</p>
