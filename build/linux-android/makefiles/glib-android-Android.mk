LOCAL_PATH := $(call my-dir)

# expat
include $(CLEAR_VARS)

LOCAL_MODULE        := glib-android
LOCAL_SRC_FILES     := lib/libglib-android-1.0.a
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/include/glib-android-1.0

include $(PREBUILT_STATIC_LIBRARY)
