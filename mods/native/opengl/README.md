# opengl mod

goome as a mod menu in an OpenGL game. Builds to `version.dll`, drop it next to the game.

It hooks `wglSwapBuffers` and `SwapBuffers` and draws goome into the back buffer right before the swap, with the `goome_gl3` backend (it falls back to `goome_gl2` on old fixed function contexts). It runs on the game's own GL context so there's nothing to set up. Insert toggles the menu.

Build: `bash ../build.sh opengl`, then put `../build/opengl/version.dll` beside the exe.

Single player and modding only, anti-cheat will ban you. See ../README.md.
