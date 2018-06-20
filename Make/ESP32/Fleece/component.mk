# ESP32 component Makefile


COMPONENT_SRCDIRS 			:= ../../../Fleece/  ../../../Fleece/Mutable  ../../../Fleece/Tree \
	  						   ../../../Fleece/DB  ../../../vendor/jsonsl  ../../../vendor/libb64
COMPONENT_ADD_INCLUDEDIRS 	:= ../../../Fleece/  ../../../Fleece/Mutable  ../../../Fleece/Tree \
							   ../../../Fleece/DB

COMPONENT_PRIV_INCLUDEDIRS 	:= ../../../vendor/jsonsl  ../../../vendor/libb64

CPPFLAGS += -D_GNU_SOURCE -DFL_EMBEDDED -DFL_ESP32 -DDEBUG=1
CFLAGS   += -Wno-unknown-pragmas  -Wno-char-subscripts
CXXFLAGS += -Wno-unknown-pragmas  -Wno-missing-field-initializers  -Wno-ignored-qualifiers \
	  		-std=gnu++11  -fexceptions  -frtti
