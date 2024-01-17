function(install_app_image)
	set(OPTION_ARGS)
	set(ONE_VALUE_ARGS
		NAME
		EXECUTABLE
		ICON
		DESTINATION
	)
	set(MULTI_VALUE_ARGS
		RESOURCES
	)
	cmake_parse_arguments(PARSE_ARGV 0 ARG "${OPTION_ARGS}" "${ONE_VALUE_ARGS}" "${MULTI_VALUE_ARGS}")

	set(APPIMAGE_FILES "${ARG_DESTINATION}/${ARG_EXECUTABLE}_AppImageFiles")
	set(APPDIR "${APPIMAGE_FILES}/AppDir")
	set(APPIMAGE_TOOL "${APPIMAGE_FILES}/AppImageTool.AppImage" CACHE INTERNAL "")
	install(CODE "
		if(NOT EXISTS \"${APPIMAGE_TOOL}\")
			file(DOWNLOAD https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${CMAKE_SYSTEM_PROCESSOR}.AppImage \"${APPIMAGE_TOOL}\" SHOW_PROGRESS)
			execute_process(COMMAND chmod +x \"${APPIMAGE_TOOL}\")
		endif()

		file(MAKE_DIRECTORY \"${APPDIR}\")
		file(WRITE \"${APPIMAGE_FILES}/AppRun\"
\"#!/bin/sh
cd \\\"$(dirname \\\"$0\\\")\\\"
./${ARG_EXECUTABLE} $@\"
		)
	")

	install(PROGRAMS "${APPIMAGE_FILES}/AppRun" DESTINATION "${APPDIR}")
	install(TARGETS "${ARG_EXECUTABLE}" RUNTIME DESTINATION "${APPDIR}")
	install(DIRECTORY "${ARG_RESOURCES}" DESTINATION "${APPDIR}")
	install(CODE "file(WRITE \"${APPDIR}/${ARG_NAME}.desktop\"
\"[Desktop Entry]
Type=Application
Name=${ARG_NAME}
Icon=${ARG_NAME}
Categories=X-None;\"
	)")
	get_filename_component(ICON_EXT "${ARG_ICON}" EXT)
	install(FILES "${ARG_ICON}" DESTINATION "${APPDIR}" RENAME "${ARG_NAME}${ICON_EXT}")
	install(CODE "execute_process(COMMAND \"${APPIMAGE_TOOL}\" \"${APPDIR}\" \"${ARG_DESTINATION}/${ARG_NAME}.AppImage\")")
endfunction()
