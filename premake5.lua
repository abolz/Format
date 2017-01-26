local build_dir = "build/" .. _ACTION

--------------------------------------------------------------------------------
solution "Libs"
    configurations { "Release", "Debug" }

    architecture "x64"

    location    (build_dir)
    objdir      (build_dir .. "/obj")

    warnings "Extra"

    configuration { "Debug" }
        targetdir (build_dir .. "/bin/Debug")

    configuration { "Release" }
        targetdir (build_dir .. "/bin/Release")

    configuration { "Debug" }
        defines { "_DEBUG" }
        symbols "On"

    configuration { "Release" }
        defines { "NDEBUG" }
        symbols "Off"
        optimize "On"
            -- On ==> -O2
            -- Full ==> -O3

    configuration { "gmake" }
        buildoptions {
--            "-march=haswell",
            "-std=c++14",
            "-Wformat",
            "-Wsign-compare",
            "-Wsign-conversion",
--            "-pedantic",
            "-fvisibility=hidden",
            "-fno-exceptions",
            "-fno-rtti",
--            "-fno-omit-frame-pointer",
--            "-ftime-report",
        }

    configuration { "gmake", "Debug", "not Windows" }
        buildoptions {
            "-fsanitize=undefined",
        }
        linkoptions {
            "-fsanitize=undefined",
        }

    configuration { "vs*" }
        buildoptions {
            "/std:c++latest",
--            "/arch:AVX2",
--            "/GR-",
        }
--        defines {
--            "_CRT_SECURE_NO_WARNINGS=1",
--            "_SCL_SECURE_NO_WARNINGS=1",
--        }

    configuration { "windows" }
        characterset "Unicode"

--------------------------------------------------------------------------------
group "Libs"

project "fmtxx"
    language "C++"
    kind "SharedLib"
    files {
        "src/Format.h",
        "src/Format_std.h",
        "src/Format.cc",
    }
    defines {
        "FMTXX_SHARED",
        "FMTXX_EXPORT",
    }
    includedirs {
        "src/",
    }
    configuration { "gmake" }
        buildoptions {
            "-Wsign-compare",
            "-Wsign-conversion",
            "-Wold-style-cast",
            "-pedantic",
        }

--------------------------------------------------------------------------------
group "Tests"

project "Test"
    language "C++"
    kind "ConsoleApp"
    files {
        "test/Test.cc",
    }
    defines {
        "FMTXX_SHARED",
    }
    includedirs {
        "src/",
    }
    links {
        "fmtxx",
    }
