all:
	cmake --build build

run:
	.\build\sub-racer\sub-racer.exe

tests:
	.\build\test\sub-racer-tests.exe