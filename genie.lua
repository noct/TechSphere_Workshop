if not _ACTION then return end

solution "TechSphere"
    configurations {"Debug", "Release"}
    platforms {"x64"}
    location  (".build/".._ACTION)
    objdir    (".build/".._ACTION.."/obj")
    targetdir (".build/".._ACTION.."/lib")
    defines {"SDL_MAIN_HANDLED"}
    links { "opengl32" }
    flags {
        "NoPCH",
        "ExtraWarnings",
        "Symbols",
        "WinMain"
    }

    configuration {"Debug"}
        defines {"DEBUG"}

    configuration {"Release"}
        defines {"NDEBUG"}
        flags   {"Optimize"}

    configuration {}

    project "WorkshopJan2019"
        kind "WindowedApp"
        language "C++"
        debugdir (".build/".._ACTION.."/lib")

        includedirs {
        }

        files {
            "Source/**.c",
            "Source/**.h"
        }
