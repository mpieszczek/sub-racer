all:
	cmake --build build

all-full-rebuild:
	cmake --preset mingw
	cmake --build build

run:
	.\build\sub-racer\sub-racer.exe

tests:
	.\build\test\sub-racer-tests.exe