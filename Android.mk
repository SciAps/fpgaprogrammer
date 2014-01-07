LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := fpgaprogram
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := ports.c micro.c lenval.c 
include $(BUILD_EXECUTABLE)

