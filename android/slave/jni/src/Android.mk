LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := rig-slave
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG
LOCAL_SRC_FILES := rig-slave.c
LOCAL_STATIC_LIBRARIES := clib cogl rut rig android_native_app_glue
LOCAL_SHARED_LIBRARIES := protobuf-c icuuc fontconfig freetype harfbuzz harfbuzz-icu

LOCAL_LDLIBS := -ldl -lGLESv3 -lEGL -llog -landroid

include $(BUILD_SHARED_LIBRARY)

$(call import-module, clib)
$(call import-module, cogl)
$(call import-module, rut)
$(call import-module, rig)
$(call import-module, protobuf-c)
$(call import-module, icu)
$(call import-module, libuv)
$(call import-module, fontconfig)
$(call import-module, freetype)
$(call import-module, harfbuzz)
$(call import-module, android/native_app_glue)
