# ESP32 component Makefile


COMPONENT_SRCDIRS 			:= ../../../Fleece/API_Impl  ../../../Fleece/Core  ../../../Fleece/Mutable \
	  						   ../../../Fleece/Support  ../../../Fleece/Tree \
							   ../../../vendor/jsonsl  ../../../vendor/libb64
COMPONENT_ADD_INCLUDEDIRS 	:= ../../../API \
							   ../../../Fleece/Core  ../../../Fleece/Mutable  ../../../Fleece/Tree \
							   ../../../Fleece/Support

COMPONENT_PRIV_INCLUDEDIRS 	:= ../../../vendor/jsonsl  ../../../vendor/libb64

CPPFLAGS += -D_GNU_SOURCE  -DFL_EMBEDDED
CFLAGS   += -Wno-unknown-pragmas  -Wno-char-subscripts
CXXFLAGS += -Wno-unknown-pragmas  -Wno-missing-field-initializers  -Wno-ignored-qualifiers \
	  		-std=gnu++11  -fexceptions  -frtti
