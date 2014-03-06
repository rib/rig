LOCAL_PATH := $(call my-dir)

# A dummy module just to be able to include the pango headers...
include $(CLEAR_VARS)

LOCAL_MODULE        	:= pango-includes
LOCAL_SRC_FILES     	:=
LOCAL_STATIC_LIBRARIES 	:= cairo-includes
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/pango-1.0

include $(BUILD_STATIC_LIBRARY)

# pango
include $(CLEAR_VARS)

LOCAL_MODULE        	:= pango
LOCAL_SRC_FILES     	:= lib/libpango-1.0.so
LOCAL_SHARED_LIBRARIES 	:= gthread gobject
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/pango-1.0

include $(PREBUILT_SHARED_LIBRARY)

# pangoft2
include $(CLEAR_VARS)

LOCAL_MODULE        	:= pangoft2
LOCAL_SRC_FILES     	:= lib/libpangoft2-1.0.so
LOCAL_SHARED_LIBRARIES 	:= pango freetype harfbuzz fontconfig
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/pango-1.0

include $(PREBUILT_SHARED_LIBRARY)

# pangocairo
include $(CLEAR_VARS)

LOCAL_MODULE        	:= pangocairo
LOCAL_SRC_FILES     	:= lib/libpangocairo-1.0.so
LOCAL_SHARED_LIBRARIES 	:= pango pangoft2 cairo
LOCAL_EXPORT_CFLAGS 	:= -I$(LOCAL_PATH)/include/pango-1.0

include $(PREBUILT_SHARED_LIBRARY)

$(call import-module, glib)
$(call import-module, fontconfig)
$(call import-module, freetype)
$(call import-module, harfbuzz)
$(call import-module, cairo)
