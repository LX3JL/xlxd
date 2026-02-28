find_path(FTD2XX_INCLUDE_DIR
        NAMES ftd2xx.h WinTypes.h
        PATHS
        /usr/local/include
        /usr/local/include/libftd2xx
        /usr/include
        /usr/include/libftd2xx
        /opt/libftd2xx/include)

set(FTD2XX_LIBNAME ftd2xx)

find_library(FTD2XX_LIBRARY
        NAMES ftd2xx
        PATHS
        /usr/local/lib
        /usr/lib
        /opt/libftd2xx/lib)

if (FTD2XX_LIBRARY)
    if (FTD2XX_INCLUDE_DIR)
        set(FTD2XX_FOUND TRUE)
        message(STATUS "Found libFTD2XX: ${FTD2XX_INCLUDE_DIR}, ${FTD2XX_LIBRARY}")
    else (FTD2XX_INCLUDE_DIR)
        set(FTD2XX_FOUND FALSE)
        message(STATUS "libFTD2XX headers NOT FOUND. Make sure to install the development headers! Please refer to the documentation for instructions.")
    endif (FTD2XX_INCLUDE_DIR)

    add_library(ftd2xx
            SHARED IMPORTED GLOBAL)

    set_target_properties(ftd2xx
            PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${FTD2XX_INCLUDE_DIR})

    set_target_properties(ftd2xx
            PROPERTIES
            IMPORTED_LOCATION ${FTD2XX_LIBRARY})

else (FTD2XX_LIBRARY)
    set(FTD2XX_FOUND FALSE)
    message(STATUS "libFTD2XX NOT FOUND.")
endif (FTD2XX_LIBRARY)

set(FTD2XX_INCLUDE_DIR
        ${FTD2XX_INCLUDE_DIR})
