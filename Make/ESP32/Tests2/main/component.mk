# ESP32 Makefile for Fleece tests (part 2)

COMPONENT_OBJS := Tests/FleeceTestsMain.o Tests/FleeceTests.o \
	Tests/ESP32Tests.o \
	Tests/MutableTests.o \
	Tests/HashTreeTests.o \
	Tests/DBTests.o

COMPONENT_SRCDIRS 			:= ../../../../Tests
COMPONENT_ADD_INCLUDEDIRS 	:= ../../../../Tests

COMPONENT_PRIV_INCLUDEDIRS 	:= ../../../../vendor/catch  ../../../../vendor/jsonsl

COMPONENT_EMBED_FILES := ../../../../Tests/1000people.fleece

CPPFLAGS += -D_GNU_SOURCE  -DFL_EMBEDDED  -DFL_ESP32  -DFL_HAVE_TEST_FILES=0  -DDEBUG=1
CFLAGS   += -Wno-unknown-pragmas  -Wno-char-subscripts
CXXFLAGS += -Wno-unknown-pragmas  -Wno-missing-field-initializers  -Wno-ignored-qualifiers \
	  		-std=gnu++11  -fexceptions # -frtti

# Prevent dead-stripping of tests
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive