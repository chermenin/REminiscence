
SDL_CFLAGS   := `sdl2-config --cflags`
SDL_LIBS     := `sdl2-config --libs`

MODPLUG_LIBS := -lmodplug
TREMOR_LIBS  := -lvorbisidec -logg
ZLIB_LIBS    := -lz

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	GL_LIBS  := -framework OpenGL
else
	GL_LIBS  := -lGL
endif

CXX=g++
CXXFLAGS += -std=c++17 -Wall -Wpedantic -Woverlength-strings -MMD $(SDL_CFLAGS) -DUSE_MODPLUG -DUSE_TREMOR -DUSE_ZLIB

SRCS = collision.cpp cpc_player.cpp cutscene.cpp decode_mac.cpp file.cpp fs.cpp game.cpp graphics.cpp main.cpp \
	menu.cpp mixer.cpp mod_player.cpp ogg_player.cpp piege.cpp protection.cpp resource.cpp resource_aba.cpp \
	resource_mac.cpp scaler.cpp screenshot.cpp seq_player.cpp sfx_player.cpp shaders.cpp \
	staticres.cpp staticres_controllers.cpp systemstub_sdl.cpp unpack.cpp util.cpp video.cpp xbrz.cpp


OBJS = $(SRCS:.cpp=.o) $(SCALERS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d) $(SCALERS:.cpp=.d)

ifeq ($(UNAME_S),Linux)
	LIBS = $(SDL_LIBS) -Wl,-Bstatic $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS) $(GL_LIBS) -Wl,-Bdynamic
else
	LIBS = $(SDL_LIBS) $(MODPLUG_LIBS) $(TREMOR_LIBS) $(ZLIB_LIBS) $(GL_LIBS)
endif

fb: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
