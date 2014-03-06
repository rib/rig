LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE        := pixman
LOCAL_SRC_FILES     := lib/libpixman-1.so
LOCAL_EXPORT_CFLAGS := -I$(LOCAL_PATH)/include/pixman-1
LOCAL_STATIC_LIBRARIES := cpufeatures

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module,android/cpufeatures)
