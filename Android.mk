LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES := libcutils libutils libbinder libstlport
LOCAL_MODULE := playxsvf
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := ports.c micro.c lenval.c 
include $(BUILD_EXECUTABLE)
