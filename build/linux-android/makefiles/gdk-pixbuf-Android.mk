LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        	:= gdk-pixbuf
LOCAL_SRC_FILES     	:= lib/libgdk_pixbuf-2.0.so
LOCAL_SHARED_LIBRARIES 	:= glib gobject tiff libjpeg libpng
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/gdk-pixbuf-2.0

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, glib)
$(call import-module, tiff)
$(call import-module, libjpeg)
$(call import-module, libpng)
