LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := editor
LOCAL_SRC_FILES := main.c
LOCAL_LDLIBS    := -llog -landroid
LOCAL_STATIC_LIBRARIES := rig cogl-pango android_native_app_glue
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := 				\
	-DG_LOG_DOMAIN=\"TestCoglHello\"	\
	-DCOGL_ENABLE_EXPERIMENTAL_2_0_API	\
	-g3 -O0					\
	$(NULL)

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,cogl)
$(call import-module,rig)

