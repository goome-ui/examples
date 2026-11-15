# vulkan mod

goome as a mod menu in a Vulkan game. Builds to `version.dll`, drop it next to the game.

It hooks the loader's `vkCreateInstance`, `vkCreateDevice`, `vkCreateSwapchainKHR` and `vkQueuePresentKHR` to gather the device, queue and swapchain images, then builds a render pass and a command buffer per image. On present it records a pass over the image being shown, draws goome with the `goome_vulkan` backend, and chains a semaphore so the present waits for it. Insert toggles the menu.

The build script fetches the Vulkan headers, they aren't checked in. Build: `bash ../build.sh vulkan`, then put `../build/vulkan/version.dll` beside the exe.

Single player and modding only, anti-cheat will ban you. See ../README.md.
