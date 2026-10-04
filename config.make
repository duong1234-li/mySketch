# Pen pressure comes from libinput (see src/ofAppTablet.cpp).
# USER_LDFLAGS is the openFrameworks hook for project libraries; plain LDFLAGS
# gets overwritten by compile.project.mk.
USER_LDFLAGS += -linput
