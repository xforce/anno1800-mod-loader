# Anno 1800 Mod Loader

| WARNING: This is currently only compatible with the Uplay Version, see issue #3 |
| --- |

The one and only mod loader for Anno 1800, supports loading of unpacked RDA files, XML auto merging and DLL based mods.

No file size limit. No more repacking. Less likely to break after updates (in general a mod should continue to work after every update, YMMV).

# Asset modding

In previous anno games there was a way to tell the game to load extacted files from disk instead of loading them  
from the RDA container. While that made it easier, it's still not a nice way to handle modding large XML files.

## XML files

This Anno 1800 Mod loader supports a few simple 'commands' to easily patch the XML to achieve pretty much whatever you want.  
Currently supported operations inside an XML file:

> This was just a quick initial implementation (~3h), very open for discussions on how to make that better or do something entirely different

- Merge
- Remove
- Add
- Replace

Lookup for all of those is done using XPath, this makes it easy and possible to only have the changes in a mod that you absolutely need instead of handling megabytes of XML files.

Example:
Adding a new zoom level

Put this in a mod folder with the game path
so this would be in `mods/new-zoom-level/data/config/game/camera.xml`

> The mods folder in a default Uplay installation has to be located at `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\mods`

```xml
<ModOp Type="add" Path="/Normal/Presets">
    <Preset ID="15" Height="140" Pitch="0.875" MinPitch="-0.375" MaxPitch="1.40" Fov="0.56" />
</ModOp>
<ModOp Type="merge" Path="/Normal/Settings">
    <Settings MaxZoomPreset="15"></Settings>
</ModOp>
```

You can find more examples in the `examples` directory.  
To test what a 'patch' you write does, you can use `xml-test`, which will simulate what the game does.

```
xml-test game_camera.xml patch.xml
```

> This will write a patched.xml file in the current directory

Original whitespace should be pretty much the same, so you can use some diff tool to see exactly what changed.

## Other files

Other file types can't be 'merged' obviously, so there we just load the version of the last mod that has that file. (Mods are loaded alphabetically).

# Installation

Head over to the releases page and download the loader.zip from the latest release.  
Unzip the contents to the location where Anno1800.exe is (For uplay default that would be C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\Bin\Win64), you will be asked to overwrite python35.dll, just accept that.

You probably also need the VS 2019 Redist https://aka.ms/vs/16/release/VC_redist.x64.exe

And that's basically it.

Mods will be loaded from `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\Anno 1800\mods` assuming default Uplay path.
For an example zoom extend mod see the `examples` directory.

# Building

You need Bazel, Visual Studio 2019 and that _should_ be it.  
You can checkout `azure-pipelines.yml` and see how it's done there.

If you want to work on new features for XML operations, you can use xmltest for testing. As that is using the same code as the actualy file loader.

# Debugging

Debugging will not be possible, the game is using Denuvo and VMProtect, I have my own tools that allow me to debug it, but I will not be sharing those publicly.  
You can however do printf aka log file debugging, not as easy and fun, but will get the job done.

# Coming soon (maybe)

- Access to the Anno python api, the game has an internal python API, I am not yet at a point where I can say how much you can do with it, but I will be exploring that in the future.
