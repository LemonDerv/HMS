#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmdShow) {
    MainWindow window(instance);
    if (!window.Create()) {
        return 0;
    }

    return window.Run(cmdShow);
}
