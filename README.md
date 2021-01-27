# Installation instructions

1. Grab the latest release from [the Releases page](https://github.com/nathanchance/WSL2-Linux-Kernel/releases).

2. Place the kernel somewhere in Windows, such as in a folder in your user folder named Linux (e.g., `C:\Users\natec\Linux\bzImage`).

3. Open a file editor such as Visual Studio Code and type the following:

```
[wsl2]
kernel =
```

After that `=`, put the full path to the kernel image with all of the `\` replaced with `\\` (e.g. `C:\\Users\\natec\\Linux\\bzImage`).

It should look something like:

```
[wsl2]
kernel = C:\\Users\\natec\\Linux\\bzImage
```

4. Save this file as `.wslconfig` in the current user's home directory (e.g. `C:\Users\natec\.wslconfig`).

5. Restart WSL with `wsl.exe --shutdown` and check that the new image has been booted with `uname -r`.

To update the kernel, continuously download the latest release from the releases page or use one of the tools mentioned in [this issue](https://github.com/nathanchance/WSL2-Linux-Kernel/issues/5).
