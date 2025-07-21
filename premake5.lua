workspace "Interstellar"
    configurations { "Debug", "Release" }
    platforms { "x86", "x64" }
    location "build"

local vcpkg_root = os.getenv("VCPKG_ROOT") or "../../vcpkg"
local function vcpkg_path(triplet, kind)
    return path.join(vcpkg_root, "installed", triplet, kind)
end

function project_generator(realm)
    project ("Interstellar_" .. realm)
        kind "SharedLib"
        language "C++"
        cppdialect "C++20"

        targetdir ("bin/%{cfg.platform}/%{cfg.buildcfg}")
        objdir ("bin-int/%{cfg.platform}/%{cfg.buildcfg}")

        defines { "INTERSTELLAR_EXTERNAL" }
        files { "entry_point.cpp", "**.cpp", "**.hpp", "**.c", "**.h" }
        includedirs { "." }

        staticruntime "on"

        if realm == "gmsv" then
            defines { "GMSV" }
        elseif realm == "gmcl" then
            defines { "GMCL" }
        end

        filter "system:windows"
            systemversion "latest"
            flags { "MultiProcessorCompile" }
            runtime "Release"
            buildoptions { "/MT" }

            filter { "system:windows", "platforms:x86" }
                targetname (realm .. "_interstellar_win32")
            
            filter { "system:windows", "platforms:x64" }
                targetname (realm .. "_interstellar_win64")

        filter "system:linux"
            staticruntime "On"
            targetprefix ""
            links { 
                "ixwebsocket",
                "sodium",
                "cpprest",
                "llhttp",
                "cpr",
                "curl",
                "ssl",
                "crypto",
                "zstd",
                "lzma",
                "bz2",
                "z",
                "fmt",
                "pthread"
            }

            filter { "system:linux", "platforms:x86" }
                includedirs { vcpkg_path("x86-linux", "include") }
                libdirs { vcpkg_path("x86-linux", "lib") }
                targetname (realm .. "_interstellar_linux")

            filter { "system:linux", "platforms:x64" }
                pic "On"
                includedirs { vcpkg_path("x64-linux", "include") }
                libdirs { vcpkg_path("x64-linux", "lib") }
                targetname (realm .. "_interstellar_linux64")

        filter "platforms:x86"
            architecture "x86"

        filter "platforms:x64"
            architecture "x86_64"

        filter "configurations:Debug"
            defines { "DEBUG" }
            symbols "On"

        filter "configurations:Release"
            defines { "NDEBUG" }
            optimize "On"
        
        filter {}
end

project_generator("gmsv")
project_generator("gmcl")

require('vstudio')

premake.override(premake.vstudio.vc2010.elements, "project", function(base, prj)
	local calls = base(prj)
	table.insertafter(calls, premake.vstudio.vc2010.project, function(prj)
        premake.w([[<PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">
    <VcpkgUseStatic>true</VcpkgUseStatic>
  </PropertyGroup>
  <PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">
    <VcpkgUseStatic>true</VcpkgUseStatic>
  </PropertyGroup>
  <PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <VcpkgUseStatic>true</VcpkgUseStatic>
  </PropertyGroup>
  <PropertyGroup Label="Vcpkg" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <VcpkgUseStatic>true</VcpkgUseStatic>
  </PropertyGroup>]])
    end)
	return calls
end)