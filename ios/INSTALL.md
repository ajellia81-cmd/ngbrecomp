# Installation and clean-install test

## Install the IPA

1. Obtain the encrypted game IPA artifact from the production workflow.
2. Extract the outer ZIP, then open the inner 7z archive with the delivery
   password. Keep the password private.
3. Extract the unsigned IPA and import it into a compatible configured host,
   or sign it through your chosen installation method.
4. Launch **Ninja Gaiden Black** in the foreground and import your own
   supported ISO/XISO or complete extracted game folder when prompted.

The extracted folder must contain the supported executable and the rest of
the disc tree. Importing the executable alone does not provide playable game
data. The Korean support capsule is included by the Korean-enabled build and
needs no separate installation.

For LiveContainer setup, consult its
[official installation guide](https://livecontainer.github.io/docs/installation).
This project does not provide certificates or establish support for every host
or iOS version. Older builds may be displayed as NGB Recomp.

## Test a completely new installation

Back up any wanted saves and settings outside the guest app's data container.
Keep the original ISO or complete game folder separately as well. Deleting an
app and its container can delete imported game data, saves, settings and caches.

In LiveContainer, remove the NGB guest app and its associated data container;
uninstalling LiveContainer itself is unnecessary. If the old container is
retained, its settings and saves can survive the IPA replacement, which is not
a clean-install test.

Import the new IPA as a new guest app, launch it and import game data again.
Allow the first import and cache preparation to finish. Check title/menu access
and New Game before restoring a save. First-run time can differ from later
launches. Keep your backup until the new installation has been checked.

## Controls

A connected controller uses the normal controller mappings. Without one, the
app supplies touch controls. Graphics, FPS display and Smooth Motion remain
available through settings. Smooth Motion requires compatible high-refresh
output and supported rendering settings; see [120FPS.md](120FPS.md).
