# Android full debug build

This packaging runs the same emulator/firmware sources as the desktop build in
an Android `NativeActivity`. It includes all nine regional TPAK files and both
64-bit ABIs (`arm64-v8a` for devices and `x86_64` for emulators).

With Android Studio's SDK, NDK and JBR installed:

```powershell
.\.venv\Scripts\python.exe tools\build_android.py
```

The APK is written to `build/android/TamaPoke-ko.1.1.5-Android-Full-debug.apk`.
The project-local debug key is generated under the ignored `build/` directory.
An APK signed by a different PC's debug key cannot update an installed copy in
place. Uninstalling also erases that app's Android save data, so keep the old
app when its save matters and sign the update with its original keystore. Back
up `build/android/debug.keystore` if this key will sign future uploaded builds.

LAN battles use UDP discovery on port 38631. Two Android devices must be on the
same Wi-Fi. For Android-to-hardware play, start LAN pairing on the ESP32, join
the `TamaPoke-XXXX` network from Android with password `tamapoke`, keep the
connection despite its lack of Internet, then start LAN pairing in the app.
Android 17 also asks for local-network access the first time LAN pairing starts;
allow it and the waiting screen continues automatically.

The same LAN screen transfers whole saves between Android, Wear OS and ESP32.
Choose SEND SAVE on the source and RECEIVE SAVE on the destination. Both peers
show a six-digit comparison code. The receiver validates every chunk and the
whole backup before offering APPLY SAVE; receiving alone never changes NVS.

Growth and the clock screen follow Android's system date/time directly. The
first fixed launch rebases older app timestamps without changing the saved
level, and a backward system-clock correction cannot become an unsigned jump.

## Wear OS / Galaxy Watch4 through Watch9

Build the standalone universal ARM Wear OS edition with:

```powershell
.\.venv\Scripts\python.exe tools\build_android.py --wear
```

The output is `build/android/TamaPoke-ko.1.1.5-WearOS-GalaxyWatch4-9-debug.apk`.
It scales the existing 466 x 466 round canvas and touch coordinates to the
device's actual square surface without changing the aspect ratio. The APK
declares `android.hardware.type.watch`, is standalone, and contains both
`armeabi-v7a` and `arm64-v8a` so that Galaxy Watch generations with different
Wear OS ABIs can select the matching native library automatically. LAN battles
and save transfer require the watch Wi-Fi to be connected to the same network
as the phone, or directly to the `TamaPoke-XXXX` hardware network.
