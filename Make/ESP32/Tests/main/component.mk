# ESP32 Makefile for Fleece tests (part 2)

COMPONENT_OBJS := Tests/FleeceTestsMain.o Tests/FleeceTests.o \
	Tests/EncoderTests.o \
	Tests/JSON5Tests.o \
	Tests/SharedKeysTests.o \
	Tests/SupportTests.o \
	Tests/ValueTests.o \
	Tests/MutableTests.o \
	Tests/HashTreeTests.o \
	Experimental/KeyTree.o

COMPONENT_SRCDIRS 			:= ../../../../Tests  ../../../../Experimental

COMPONENT_PRIV_INCLUDEDIRS 	:= ../../../../vendor/catch  ../../../../vendor/jsonsl  ../../../../Experimental

COMPONENT_EMBED_FILES := ../../../../Tests/1000people.fleece

CPPFLAGS += -D_GNU_SOURCE  -DFL_EMBEDDED  -DFL_ESP32  -DFL_HAVE_FILESYSTEM=0  -DDEBUG=1
CFLAGS   += -Wno-unknown-pragmas  -Wno-char-subscripts
CXXFLAGS += -Wno-unknown-pragmas  -Wno-missing-field-initializers  -Wno-ignored-qualifiers \
	  		-std=gnu++11  -fexceptions # -frtti

# Prevent dead-stripping of tests
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive