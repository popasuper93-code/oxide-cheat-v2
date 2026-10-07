LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cheat
LOCAL_SRC_FILES := cheat.cpp
LOCAL_LDLIBS := -llog -ldl -landroid -lEGL -lGLESv2
include $(BUILD_SHARED_LIBRARY)
