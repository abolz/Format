local build_dir = "build/" .. _ACTION

--------------------------------------------------------------------------------
solution "Format"
    configurations { "release", "debug" }
    architecture "x64"

    location    (build_dir)
    objdir      (build_dir .. "/obj")

    warnings "Extra"

    -- exceptionhandling "Off"
    -- rtti "Off"

    configuration { "debug" }
        targetdir (build_dir .. "/bin/debug")

    configuration { "release" }
        targetdir (build_dir .. "/bin/release")

    configuration { "debug" }
        defines { "_DEBUG" }
        symbols "On"

    configuration { "release" }
        defines { "NDEBUG" }
        symbols "On"
        optimize "Full"
            -- On ==> -O2
            -- Full ==> -O3

    configuration { "gmake" }
        buildoptions {
            "-march=native",
            "-std=c++11",
            "-Wformat",
--            "-Wsign-compare",
--            "-Wsign-conversion",
--            "-pedantic",
--            "-fvisibility=hidden",
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
            -- "/std:c++latest",
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
        "src/**.h",
        "src/**.cc",
    }
    defines {
        "FMTXX_SHARED=1",
        "FMTXX_EXPORT=1",
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

-- project "fmt"
--    language "C++"
--    kind "SharedLib"
--    files {
--        "ext/fmt/**.h",
--        "ext/fmt/**.cc",
--    }
--    defines {
--        "FMT_SHARED=1",
--        "FMT_EXPORT=1",
--    }
--    includedirs {
--        "ext/",
--    }

--------------------------------------------------------------------------------
group "Tests"

project "Test"
    language "C++"
    kind "ConsoleApp"
    files {
        "test/Test.cc",
    }
    defines {
        "FMTXX_SHARED=1",
    }
    includedirs {
        "src/",
    }
    links {
        "fmtxx",
    }

function AddExampleProject(name)
    project (name)
        language "C++"
        kind "ConsoleApp"
        files {
            "test/" .. name .. ".cc",
        }
        defines {
            "FMTXX_SHARED=1",
        }
        includedirs {
            "src/",
        }
        links {
            "fmtxx",
        }
end

AddExampleProject("Example1")
AddExampleProject("Example2")
AddExampleProject("Example3")
AddExampleProject("Example4")
AddExampleProject("Example5")

-- Doesn't work with MinGW (std::condition_variable not implemented...)
-- project "Benchmark"
--     language "C++"
--     kind "ConsoleApp"
--     files {
--         "test/Bench.cc",
--         "test/ext/benchmark/include/benchmark/*.h",
--         "test/ext/benchmark/src/*.cc",
--     }
--     defines {
--         "HAVE_STD_REGEX=1",
--         "FMTXX_SHARED=1",
--     }
--     includedirs {
--         "src/",
--         "test/ext/",
--         "test/ext/benchmark/include/",
--     }
--     links {
--         "fmtxx",
--     }
--     configuration { "vs*" }
--         links {
--             "shlwapi",
--         }
--     configuration { "not vs*" }
--         links {
--             "pthread",
--         }
