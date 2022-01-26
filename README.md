# About

This is a custom kernel for Microsoft's [Windows Subsystem for Linux 2](https://docs.microsoft.com/en-us/windows/wsl/install-win10), based on the bleeding edge [linux-next](https://git.kernel.org/pub/scm/linux/kernel/git/next/linux-next.git/) and compiled with [clang](https://clang.llvm.org/) with Link Time Optimization and Control Flow Integrity. Follow the documentation on Microsoft's website to enable or upgrade to WSL2 for your device and distribution before attempting to install this kernel, as it will not work with WSL1.

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

If your username has a space in it (for example, `nathan chance`), do not attempt to escape it with `\` or `"`:

```
[wsl2]
kernel = C:\\Users\\nathan chance\\Linux\\bzImage
```

4. Save this file as `.wslconfig` in the current user's home directory (e.g. `C:\Users\natec\.wslconfig`).

5. Restart WSL with `wsl.exe --shutdown` and check that the new image has been booted with `uname -r`.

To update the kernel, continuously download the latest release from the releases page or use one of the tools mentioned in [this issue](https://github.com/nathanchance/WSL2-Linux-Kernel/issues/5).
