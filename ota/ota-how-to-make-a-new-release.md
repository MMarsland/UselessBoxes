## OTA update steps for this project

Now that GitHub Releases OTA is wired in, the flow is:


---

## 1) Make your firmware changes

Edit the code as needed.

---

## 2) Bump the firmware version

In [Useless_Boxes.cpp](vscode-file://vscode-app/c:/Users/mmars/AppData/Local/Programs/Microsoft%20VS%20Code/cfbea10c5f/resources/app/out/vs/code/electron-browser/workbench/workbench.html), update:

to the new version, for example:

> 114 : constexpr char CURRENT_FW_VERSION[] = "v1.0.1"; // Bump this for each release

---

## 3) Run the Powershell OTA Scropt

Run this command which handles the release:

```
powershell -ExecutionPolicy Bypass -File "c:\Users\mmars\Desktop\Personal\Storage Hold\Code\UselessBoxes\PlatformIO\scripts\release_ota.ps1" -CommitManifest -Push -CreateGitHubRelease
```

---

## Quick checklist

### Every OTA release:

1. Change code
2. bump [CURRENT_FW_VERSION](vscode-file://vscode-app/c:/Users/mmars/AppData/Local/Programs/Microsoft%20VS%20Code/cfbea10c5f/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
3. build both envs
4. upload both `.bin` files to a GitHub Release
5. update [michael.txt](vscode-file://vscode-app/c:/Users/mmars/AppData/Local/Programs/Microsoft%20VS%20Code/cfbea10c5f/resources/app/out/vs/code/electron-browser/workbench/workbench.html) and [trevor.txt](vscode-file://vscode-app/c:/Users/mmars/AppData/Local/Programs/Microsoft%20VS%20Code/cfbea10c5f/resources/app/out/vs/code/electron-browser/workbench/workbench.html)
6. push manifest changes
