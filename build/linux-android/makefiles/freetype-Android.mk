LOCAL_PATH := $(call my-dir)

# freetype
include $(CLEAR_VARS)

LOCAL_MODULE        	:= freetype
LOCAL_SRC_FILES     	:= lib/libfreetype.so
LOCAL_SHARED_LIBRARIES 	:= libpng
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/freetype2

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, libpng)
