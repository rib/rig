LOCAL_PATH := $(call my-dir)

# expat
include $(CLEAR_VARS)

LOCAL_MODULE        := expat
LOCAL_SRC_FILES     := lib/libexpat.a
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/include/expat

include $(PREBUILT_STATIC_LIBRARY)
