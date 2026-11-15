TARGET := build/object-method-target
SOURCES := target.cpp
HEADERS := objects.hpp
CXXFLAGS ?= -std=c++17 -g -O0 -fno-inline

ifeq ($(shell uname -s),Linux)
CXXFLAGS += -fno-pie
LDFLAGS += -no-pie
endif

include ../object-lesson.mk
