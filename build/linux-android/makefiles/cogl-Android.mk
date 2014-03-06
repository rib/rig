LOCAL_PATH := $(call my-dir)

# A dummy module just to be able to include the cairo headers...
include $(CLEAR_VARS)

LOCAL_MODULE        	:= cogl-includes
LOCAL_SRC_FILES     	:=
LOCAL_STATIC_LIBRARIES 	:= pango-includes
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cogl2

include $(BUILD_STATIC_LIBRARY)

# cogl
include $(CLEAR_VARS)

LOCAL_MODULE        	:= cogl
LOCAL_SRC_FILES     	:= lib/libcogl2.so
LOCAL_SHARED_LIBRARIES 	:= glib gdk-pixbuf
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cogl2

include $(PREBUILT_SHARED_LIBRARY)

# cogl-pango
include $(CLEAR_VARS)

LOCAL_MODULE        	:= cogl-pango2
LOCAL_SRC_FILES     	:= lib/libcogl-pango2.so
LOCAL_SHARED_LIBRARIES 	:= cogl gobject pangocairo
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cogl2

include $(PREBUILT_SHARED_LIBRARY)


# cogl-path
include $(CLEAR_VARS)

LOCAL_MODULE        	:= cogl-path
LOCAL_SRC_FILES     	:= lib/libcogl-path.so
LOCAL_SHARED_LIBRARIES 	:= cogl
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/cogl2

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, glib)
$(call import-module, pango)
$(call import-module, gdk-pixbuf)
