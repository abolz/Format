local build_dir = "build/" .. _ACTION

--------------------------------------------------------------------------------
solution "Libs"
    configurations { "release", "debug" }

    architecture "x64"

    location    (build_dir)
    objdir      (build_dir .. "/obj")

    warnings "Extra"

    configuration { "debug" }
        targetdir (build_dir .. "/bin/debug")

    configuration { "release" }
        targetdir (build_dir .. "/bin/release")

    configuration { "debug" }
        defines { "_DEBUG" }
        symbols "On"

    configuration { "release" }
        defines { "NDEBUG" }
        symbols "Off"
        optimize "Full"
            -- On ==> -O2
            -- Full ==> -O3

    configuration { "gmake" }
        buildoptions {
            "-march=native",
            "-std=c++14",
            "-Wformat",
--            "-Wsign-compare",
--            "-Wsign-conversion",
--            "-pedantic",
--            "-fvisibility=hidden",
            "-fno-exceptions",
--            "-fno-rtti",
--            "-fno-omit-frame-pointer",
--            "-ftime-report",
        }

--    configuration { "gmake", "debug", "linux" }
--        buildoptions {
--            "-fno-omit-frame-pointer",
--            "-fsanitize=undefined",
--            "-fsanitize=address",
----            "-fsanitize=memory",
----            "-fsanitize-memory-track-origins",
--        }
--        linkoptions {
--            "-fsanitize=undefined",
--            "-fsanitize=address",
----            "-fsanitize=memory",
--        }

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
        "src/double-conversion/**.h",
        "src/double-conversion/**.cc",
        "src/Dtoa.h",
        "src/Dtoa.cc",
        "src/Format.h",
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
            "-fvisibility=hidden",
        }

project "fmtxx-static"
    language "C++"
    kind "StaticLib"
    files {
        "src/double-conversion/**.h",
        "src/double-conversion/**.cc",
        "src/Dtoa.h",
        "src/Dtoa.cc",
        "src/Format.h",
        "src/Format.cc",
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
            "-fvisibility=hidden",
        }

project "fmt"
    language "C++"
    kind "SharedLib"
    files {
        "test/ext/fmt/fmt/*.cc",
        "test/ext/fmt/fmt/*.h",
    }
    defines {
        "FMT_SHARED",
        "FMT_EXPORT",
    }
    includedirs {
        "test/ext/fmt/",
    }

project "fmt-static"
    language "C++"
    kind "StaticLib"
    files {
        "test/ext/fmt/fmt/*.cc",
        "test/ext/fmt/fmt/*.h",
    }
    includedirs {
        "test/ext/fmt/",
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

project "TestBignum"
    language "C++"
    kind "ConsoleApp"
    files {
        "test/TestBignum.cc",
    }
    links {
        "fmtxx-static",
    }

project "TestPerf"
    language "C++"
    kind "ConsoleApp"
    files {
        "test/TestPerf.cc",
    }
    defines {
        "FMTXX_SHARED",
        "FMT_SHARED"
    }
    includedirs {
        "src/",
        "test/ext/",
        "test/ext/fmt/",
    }
    links {
        "fmtxx",
        "fmt",
    }
