#pragma once

#if defined(_WIN32)
#    if defined(MINE_SHARED_BUILD)
#        if defined(MINE_BUILDING_MINE_CORE)
#            define MINE_API __declspec(dllexport)
#        else
#            define MINE_API __declspec(dllimport)
#        endif
#    else
#        define MINE_API
#    endif
#else
#    if defined(MINE_SHARED_BUILD)
#        define MINE_API __attribute__((visibility("default")))
#    else
#        define MINE_API
#    endif
#endif