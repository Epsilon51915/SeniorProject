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

## System environment variables

In order for CMake to properly find both Vulkan and VCPKG, your system environment variables need to be setup properly.

1. Vulkan: Installer for SDK should set these up. However, just in case, set both **VULKAN_SDK** and **VK_SDK_PATH** to be the instillation directory of your Vulkan
2. We are using Vulkan version 1.4.363.0
3. VCPKG: Set **VCPKG_ROOT** to the instillation directory of VCPKG.

## Adding dependecies

If you need to add depencies, use VS's developer powershell and use:
```bash
vcpkg add port <package_name>
```

This will automatically add it to both vcpkg.json and vcpkg-configuration.json. You then need to add the correct commands to CMakeLists.txt
**When you push these to main always make sure any conflicts are resolved so we can all build the project**

Inside **vcpkg.json** will be your new depencies, it helps to add versioning to ensure everything is up-to-date.
Pay attention to how vcpkg tells you how to use it in CMake. You usually need to link the library against the project inside **CMakeLists.txt**
for example:
```bash
target_link_libraries("Framework" PRIVATE VulkanCppModule glfw glm::glm)
```
This is how the current libraries are linked, and where you will add another library normally.

**NOTE:** Vulkan is installed seperately with the VulkanSDK from LunarG. The CMakeLists.txt is configured for that.
**NOTE:** Ensure when running that command line is within the project directory, specifically where the vcpkg.json files are located

## Troubleshooting
If CMake isn't compiling the project:
1. Restart PC after changing environment variables(sometimes helps)
2. Your VS instillation has CMake, or points to your local copy(shouldn't be needed)
3. Your CMake has the correct vcpkg(your own instillation, not VS's), this can be checked by looking at **CMAKE_TOOLCHAIN_FILE** in CMake cache
4. If something is going wrong with CMAKE you most likely need to completely **Delete and Regenerate Cache**. VS will usually notify you about this near the top of the screen.
5. We are using VS 2026 which introduces some major structure changes from VS 2022. If you only wish to use 2022, you will most likely need your own **CMakeUserPresets.json** to set up CMake

## Got a different package manager or use Linux?
Create your own **CMakeUserPresets.json** and add what you need for it to work.
Then also add the equivalent of the vcpkg.json/vcpkg-configuration.json to the project.
The CMakeLists.txt should still work with other package managers but no testing has been done.