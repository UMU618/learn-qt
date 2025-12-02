# 编译说明

1. 安装依赖

由于本程序依赖 Vulkan，所以需要先安装它。这里使用 vcpkg 管理：

```sh
vcpkg install qt5-base[vulkan] --recurse
```

2. 安装 glslc

这里使用 vcpkg 管理：

```sh
vcpkg install shaderc
```

3. 编译 .spv 资源

进入 res 目录，按照系统运行脚本，比如 Windows 下，可直接双击“compile_shaders.cmd”。
