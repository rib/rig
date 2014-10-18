LOCAL_PATH := $(call my-dir)

RIG_PATH := $(LOCAL_PATH)/../../../../..
include $(RIG_PATH)/rig/Makefile.sources

# Slave
include $(CLEAR_VARS)

RIG_PATH := $(LOCAL_PATH)/../../../../..

LOCAL_MODULE := rig-slave
LOCAL_C_INCLUDES := $(RIG_PATH)/rig $(RIG_PATH)/rig/protobuf-c-rpc
#$(RIG_PATH) $(RIG_PATH)/clib/src $(RIG_PATH)/rut $(RIG_PATH)/rig $(RIG_PATH)/rig/protobuf-c-rpc
LOCAL_CFLAGS := -DRIG_ENABLE_DEBUG -DICU_DATA_DIR=\"/icu\" -DU_DISABLE_RENAMING=1
LOCAL_SRC_FILES := \
	$(filter-out %.h, \
	    $(subst $(LOCAL_PATH)/,, \
			    $(addprefix $(RIG_PATH)/rig/,$(rig_common_sources)) \
			    $(addprefix $(RIG_PATH)/rig/,$(rig_slave_sources)) \
	     ) \
	 )
LOCAL_STATIC_LIBRARIES := clib rut cogl
LOCAL_SHARED_LIBRARIES := protobuf-c icuuc fontconfig freetype harfbuzz harfbuzz-icu

LOCAL_LDLIBS := -ldl -lGLESv3 -llog -landroid

include $(BUILD_SHARED_LIBRARY)

$(call import-module, clib)
$(call import-module, cogl)
$(call import-module, rut)
$(call import-module, protobuf-c)
$(call import-module, icu)
$(call import-module, libuv)
$(call import-module, fontconfig)
$(call import-module, freetype)
$(call import-module, harfbuzz)
