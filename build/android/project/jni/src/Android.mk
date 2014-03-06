LOCAL_PATH := $(call my-dir)

RIG_PATH := $(LOCAL_PATH)/../../../../..
include $(RIG_PATH)/rig/Makefile.sources

# Shared utilities...
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rut
LOCAL_C_INCLUDES := $(RIG_PATH)/rut
LOCAL_CFLAGS := -DUSE_SDL -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := \
    $(subst $(LOCAL_PATH)/,, \
	  	$(wildcard $(RIG_PATH)/rut/rut*.c) \
		$(RIG_PATH)/rut/fmemopen.c \
		$(RIG_PATH)/rut/rply.c \
     )

LOCAL_SHARED_LIBRARIES += rut sdl2 pango pangocairo cogl cogl-pango2 cogl-path glib gmodule gobject gio gdk-pixbuf

LOCAL_LDLIBS := -ldl -lGLESv2 -llog -landroid

include $(BUILD_SHARED_LIBRARY)

# Slave
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rig-slave
LOCAL_C_INCLUDES := $(RIG_PATH)/rut $(RIG_PATH)/rig $(RIG_PATH)/rig/protobuf-c-rpc
LOCAL_CFLAGS := -DUSE_SDL -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := \
	$(filter-out %.h, \
    	$(subst $(LOCAL_PATH)/,, \
	  		$(addprefix $(RIG_PATH)/rig/,$(rig_common_sources)) \
	  		$(addprefix $(RIG_PATH)/rig/,$(rig_slave_sources)) \
     	 ) \
	  ) \
	SDL_android_main.c \
	rig-slave-android.c
LOCAL_SHARED_LIBRARIES += rut sdl2 pango pangocairo cogl cogl-pango2 cogl-path glib gmodule gobject gio gdk-pixbuf protobuf-c

LOCAL_LDLIBS := -ldl -lGLESv2 -llog -landroid

include $(BUILD_SHARED_LIBRARY)


# Simulator
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rig-simulator
LOCAL_C_INCLUDES := $(RIG_PATH)/rut $(RIG_PATH)/rig $(RIG_PATH)/rig/protobuf-c-rpc
LOCAL_CFLAGS := -DRIG_SIMULATOR_ONLY -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := \
	$(filter-out %.h, \
    	$(subst $(LOCAL_PATH)/,, \
	  		$(addprefix $(RIG_PATH)/rig/,$(rig_common_sources)) \
     	 ) \
	  ) \
	 rig-simulator-android.c
LOCAL_SHARED_LIBRARIES := rut cogl-includes protobuf-c \
	glib pango pangocairo cogl cogl-pango2 cogl-path glib gmodule gobject gio gdk-pixbuf #TODO remove these deps


LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)


$(call import-module, glib)
$(call import-module, gdk-pixbuf)
$(call import-module, pango)
$(call import-module, cogl)
$(call import-module, sdl2)
$(call import-module, protobuf-c)
