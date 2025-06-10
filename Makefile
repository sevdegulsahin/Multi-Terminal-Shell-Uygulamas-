# Derleyici ve bayraklar
CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gtk+-3.0`
LIBS = -lrt `pkg-config --libs gtk+-3.0`

# Nesne dosyaları
OBJS = view.o controller.o model.o

# Hedef (tüm programı derlemek için)
all: shell

# Shell programını derle
shell: $(OBJS)
	$(CC) -o shell $(OBJS) $(LIBS)

# Nesne dosyalarını derleme kuralı
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Temizleme hedefi (ara dosyaları sil)
clean:
	rm -f *.o shell

# Phony hedefler
.PHONY: all clean