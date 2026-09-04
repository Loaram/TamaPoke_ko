# Android full debug build

This packaging runs the same emulator/firmware sources as the desktop build in
an Android `NativeActivity`. It includes all nine regional TPAK files and both
64-bit ABIs (`arm64-v8a` for devices and `x86_64` for emulators).

With Android Studio's SDK, NDK and JBR installed:

```powershell
.\.venv\Scripts\python.exe tools\build_android.py
```

The APK is written to `build/android/TamaPoke-ko.1.1.1-Android-Full-debug.apk`.
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

## Wear OS / Galaxy Watch4 Classic

Build the standalone ARM64 Wear OS edition with:

```powershell
.\.venv\Scripts\python.exe tools\build_android.py --wear
```

The output is `build/android/TamaPoke-ko.1.1.1-WearOS-GalaxyWatch4-debug.apk`.
It targets the 396 x 396 (42 mm) and 450 x 450 (46 mm) round displays by scaling
the existing 466 x 466 round canvas without changing its aspect ratio. The APK
declares `android.hardware.type.watch`, is standalone, and contains only the
Galaxy Watch4-compatible `arm64-v8a` native library. LAN battles require the
watch Wi-Fi to be connected to the same network as the phone, or directly to
the `TamaPoke-XXXX` hardware network.
