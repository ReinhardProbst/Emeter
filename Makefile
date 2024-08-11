CFLAGS        = -std=c2x -Ofast -fdiagnostics-color=always -Wfatal-errors -Wall

                   
#Enable if e.g. custom boost, rapidjson, fmt or tbb include dir
#CPPFLAGSCUSTOM  = -I/opt/local/include

#LDLIBS          = -lboost_system -lboost_chrono -lboost_thread -lpthread -ltbb -lstdc++fs

#Enable if e.g. custom boost, rapidjson, fmt or tbb lib dir
#LDFLAGSCUSTOM   = -L/opt/local/lib

FILES           = emeter.c
TARGET          = emeter
STRIP           = strip -s
ECHO            = echo

.PHONY: help clean ctags

help:
	@$(ECHO) "Available targets are:"
	@$(ECHO) "make $(TARGET)"
	@$(ECHO) "make clean"

$(TARGET): $(FILES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $?

clean:
	-$(RM) $(TARGET)

#valgrind: $(TARGET)
#	@valgrind --version
#	valgrind --leak-check=full --track-origins=yes --xml=yes --xml-file=ValgrindOut.xml "./$(TARGET)_debug"

#cachegrind: $(TARGET)
#	@valgrind --version
#	valgrind --tool=cachegrind "./$(TARGET)_release"

#ctags:
#	@ctags -R .
	
# E.g. run with custom lib path: LD_LIBRARY_PATH="/opt/local/lib" ./$(TARGET)
