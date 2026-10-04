# Pen pressure comes from libinput (see src/ofAppTablet.cpp).
# USER_LDFLAGS is the openFrameworks hook for project libraries; plain LDFLAGS
# gets overwritten by compile.project.mk.
USER_LDFLAGS += -linput

# This openFrameworks build uses GLFW's X11 backend under the Wayland desktop.
PLATFORM_RUN_COMMAND = cd bin && env -u WAYLAND_DISPLAY GDK_BACKEND=x11 ./$(BIN_NAME)
