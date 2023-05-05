#idf_component_register(SRCS main.c
#gui.c
#)
set(COMPONENT_SRCS "main.c"
#"oled12864/o12864.c" "oled12864/ofont.o"
"gui.c"
"logo.c"
"ina226/ina226.c"
#"usb/myusb.c" 
"keys/mykeys.c"
"uart0/uart0.c"
"mytimer/mytimer.c"
)
set(COMPONENT_ADD_INCLUDEDIRS 
"keys"
"ina226"
"uart0"
"mytimer"
)
register_component()