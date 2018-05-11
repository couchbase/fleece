# ESP32 component Makefile

COMPONENT_SRCDIRS 			:= ../../../Tests/
COMPONENT_ADD_INCLUDEDIRS 	:= ../../../Tests/

COMPONENT_PRIV_INCLUDEDIRS 	:= ../../../vendor/catch  ../../../vendor/jsonsl

CPPFLAGS += -D_GNU_SOURCE  -DFL_EMBEDDED  -DFL_HAVE_TEST_FILES=0  -DDEBUG=1
CFLAGS   += -Wno-unknown-pragmas  -Wno-char-subscripts
CXXFLAGS += -Wno-unknown-pragmas  -Wno-missing-field-initializers  -Wno-ignored-qualifiers \
	  		-std=gnu++11  -fexceptions # -frtti
