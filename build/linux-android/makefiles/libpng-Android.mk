LOCAL_PATH := $(call my-dir)

# libpng
include $(CLEAR_VARS)

LOCAL_MODULE        := libpng
LOCAL_SRC_FILES     := lib/libpng16.so
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/include/libpng16

include $(PREBUILT_SHARED_LIBRARY)
