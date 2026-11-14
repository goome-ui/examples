<h1 align="center">🛠 Contributing to goome examples 🛠</h1>

<p align="center">
Thanks for wanting to add something! A good example is small, does one thing, and someone can read all of it in a few minutes and copy it into their own project.
</p>

<details>
<summary align="center"><b>📋 The short version 📋</b></summary>

<p align="center">
• One example per PR, one idea per example<br>
• Open your PR against the dev branch, not main<br>
• Its own folder, a short README and a <code>shot.png</code><br>
• A <code>test.txt</code> so CI can run it on the test backend<br>
• Builds on every OS with nothing but goome<br>
• Dont touch the protected files, a bot closes the PR if you do<br>
• CI has to pass
</p>
</details>

<details>
<summary align="center"><b>📁 Adding an example 📁</b></summary>

<p align="center">
Make a folder in the language you used, named after what it does:<br> <br>
<b>C</b> - <code>c/your_thing/main.c</code>, one file, <code>#include "goome.h"</code> and nothing else from outside the C standard library. <code>scripts/build-all.sh</code> builds every folder in <code>c/</code> on its own<br>
<b>C#</b> - <code>csharp/YourThing/YourThing.csproj</code> + <code>Program.cs</code>. Copy the csproj from another example, it just imports <code>../goome.props</code><br>
<b>Python</b> - <code>python/your_thing/your_thing.py</code>, standard library + goome only<br> <br>
Then put these next to it:<br> <br>
<b>README.md</b> - copy one from another example and change it. What it is and how to use and build it<br>
<b>test.txt</b> - a goome test script, the CI entry for your example. It fakes the screen and what the user does, and has to end with <code>end N</code> so the program gets a quit. Your example has to quit when it gets <code>GOOME_QUIT</code><br>
<b>shot.png</b> - a screenshot from that test run, see below<br> <br>
Examples that use offscreen mode dont need a test.txt, they just have to finish by themselves and write their picture into the folder they run in.
</p>
</details>

<details>
<summary align="center"><b>🧪 Testing and the screenshot 🧪</b></summary>

<p align="center">
Build and run everything on the test backend, no window gets opened:
</p>

```sh
scripts/build-all.sh
scripts/run-all.sh
```

<p align="center">
On Windows its <code>scripts\build-all.ps1</code> and <code>scripts\run-all.ps1</code>. Every frame your example drew lands in <code>build/run/&lt;language&gt;/&lt;name&gt;/</code> as a ppm.<br> <br>
<code>python3 tools/shots.py c/your_thing</code> turns the last frame into <code>shot.png</code> (it needs Pillow). Want a different frame? Put a line like <code>shot 12</code> in your test.txt. Dont touch up the screenshot by hand, it should be what goome drew.<br> <br>
The test script is in the c-goome docs, the common lines are <code>screen 640x360 1</code>, <code>3 click 120,80</code>, <code>4 key hello\n</code>, <code>5 hotkey alt+h</code>, <code>6 hover 200,100</code>, <code>7 close</code>, <code>8 drop 300,200 file.png</code> and <code>end 10</code>.
</p>
</details>

<details>
<summary align="center"><b>✨ What makes a good one ✨</b></summary>

<p align="center">
Keep it to a couple hundred lines at most. If it needs more it's probably two examples. Show one thing goome does and make it something a person would actually use, not a wall of every widget. It also has to work on every OS, CI checks Linux, Windows and two Macs.<br> <br>
• <b>No extra dependencies</b> - goome and the standard library, nothing to install<br>
• <b>No comments in the code</b> - good names say it, the README says the rest
</p>
</details>

<details>
<summary align="center"><b>🌿 Branches 🌿</b></summary>

<p align="center">
Everything lands on dev first, moves to beta after a while, then main. I do the moving, you only ever PR into dev.
</p>
</details>

<details>
<summary align="center"><b>🚫 What gets closed automatically 🚫</b></summary>

<p align="center">
A bot checks every PR and closes it with a comment if it hits any of these. Don't take it personally, fix it and open a new one.<br> <br>
• <b>Not against dev</b><br>
• <b>Anything in <code>.github/</code></b> - thats CI and these rules, a PR could turn them off for itself<br>
• <b>Anything in <code>scripts/</code></b> - people run those on their own computers<br>
• <b><code>LICENSE</code></b> - thats not something a PR changes<br>
• <b>Anything in <code>vendor/</code></b> - the goome copy and its <code>backends/</code> only get updated from c-goome with <code>tools/update_goome.sh</code><br>
• <b>The native mod build scripts and <code>mods/native/common/</code></b> - those build and inject a DLL, so only I change them. A new example that uses a mod is welcome, the plumbing isn't<br>
• <b>Binaries or prebuilt libs</b> - dlls, exes, zips and the like. Examples build from source<br>
• <b>Files over 1 MB</b> - keep code and screenshots small<br> <br>
Renaming or moving any of those counts too. Think one of them is wrong? <a href="https://github.com/goome-ui/examples/issues">Open an issue</a> and I'll fix it.
</p>
</details>

<details>
<summary align="center"><b>⚖️ License ⚖️</b></summary>

<p align="center">
Everything here is GPL-3.0, same as goome. By opening a PR you agree your example gets released under it too.
</p>
</details>
