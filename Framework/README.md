# Framework Building and Running

Framework uses CMake and VCPKG in order to run, both should be already configured properly(hopefully).
environment variables to vcpkg is needed

## VCPKG setup

```bash
git clone https://github.com/microsoft/vcpkg.git
```

```bash
cd vcpkg; .\bootstrap-vcpkg.bat
```

Add this instillation to your environment variables as **VCPKG_ROOT**

## Adding dependecies

If you need to add depencies, use VS's developer powershell and use:
```bash
vcpkg add port <package_name>
```

This will automatically add it to both vcpkg.json and vcpkg-configuration.json. You then need to add the correct commands to CMakeLists.txt
**When you push these to main always make sure any conflicts are resolved so we can all build the project**

**NOTE:** Vulkan is installed seperately with the VulkanSDK. The CMakeLists.txt is configured for that.

## Troubleshooting
If CMake isn't compiling the project:
1. Restart PC after changing environment variables(sometimes helps)
2. Your VS instillation has CMake, or points to your local copy(shouldn't be needed)
3. Your CMake has the correct vcpkg(your own instillation, not VS's), this can be checked by looking at **CMAKE_TOOLCHAIN_FILE** in CMake cache

## Got a different package manager?
Create your own **CMakeUserPresets.json** and add what you need for it to work.
Then also add the equivalent of the vcpkg.json/vcpkg-configuration.json to the project.
The CMakeLists.txt should still work with other package managers but no testing has been done.