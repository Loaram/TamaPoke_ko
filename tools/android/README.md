# Android full debug build

This packaging runs the same emulator/firmware sources as the desktop build in
an Android `NativeActivity`. It includes all nine regional TPAK files and both
64-bit ABIs (`arm64-v8a` for devices and `x86_64` for emulators).

With Android Studio's SDK, NDK and JBR installed:

```powershell
.\.venv\Scripts\python.exe tools\build_android.py
```

The APK is written to `build/android/TamaPoke-ko.1.1.0-Android-Full-debug.apk`.
The project-local debug key is generated under the ignored `build/` directory.
An APK signed by a different PC's debug key cannot update an installed copy in
place. Uninstalling also erases that app's Android save data, so keep the old
app when its save matters and sign the update with its original keystore. Back
up `build/android/debug.keystore` if this key will sign future uploaded builds.
