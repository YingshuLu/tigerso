SHELL = /bin/sh
CC := g++

WORKPATH := $(shell cd .. && pwd)
PROJECT := httpd
OUTPUT := $(WORKPATH)/output
BUILDDIR := $(OUTPUT)/Build/objs

ALLDIRS := $(shell ls -R $(WORKPATH) | grep '^\/.*$$' | grep -v "test" | awk '{gsub(":","");print}')
#vpath %.cpp $(ALLDIRS)
#vpath %.h  $(ALLDIRS)
SRCS := $(foreach dir, $(ALLDIRS), $(wildcard $(dir)/*.cpp))
SRCS += $(foreach dir, $(ALLDIRS), $(wildcard $(dir)/*.c))
SRCS += $(wildcard ./main.cpp)
INCS := $(foreach dir, $(ALLDIRS), $(wildcard $(dir)/*.h))
INCS += $(foreach dir, $(ALLDIRS), $(wildcard $(dir)/*.hpp))
OLDOBJS := $(patsubst %.cpp, %.o, $(SRCS))
OBJSLIST := $(notdir $(OLDOBJS))
OBJS := $(foreach obj, $(OBJSLIST), $(BUILDDIR)/$(obj))

DEPY := http_parser

SHM_FLAG := -lrt
PTHREAD_FLAG := -lpthread
CPPFLAGS := -fPIC -shared
LIBFLAGS := -L$(LIBPATH) -l$(DEPY) $(PTHREAD_FLAG) $(SHM_FLAG)
INCFLAGS := -I $(WORKPATH)

TARGET := $(OUTPUT)/$(PROJECT)
all:$(TARGET)

$(TARGET):$(SRCS)
	$(CC) -w -g $(INCFLAGS) $^ $(LIBFLAGS) -o $@ 

.PHONY: clean cleanall print

print:
	@echo TARGET=$(TARGET)
	@echo OBJSLIST=$(OBJSLIST)
	@echo OBJS=$(OBJS)
	@echo SRCS=$(SRCS)
	@echo PATH=$(PATH)

clean:
	-rm -f $(OBJS) $(OLDOBJS)

cleanall: clean
	-rm -f $(TARGET)
