#include <iostream>
#include <thread>
#include <chrono>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <atomic>
#include <ncurses.h>
#include <cstdio> // Para popen e pclose
#include <cstring> // Para strcpy e strcat

std::atomic<bool> paused(true); // Inicialmente pausado
std::atomic<bool> running(true); // Controla o loop principal

void clickMouse() {
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Error: Could not open display." << std::endl;
        return;
    }

    XEvent event;
    event.type = ButtonPress;
    event.xbutton.button = Button2;
    event.xbutton.same_screen = True;
    XQueryPointer(display, RootWindow(display, DefaultScreen(display)),
                  &event.xbutton.root, &event.xbutton.window,
                  &event.xbutton.x_root, &event.xbutton.y_root,
                  &event.xbutton.x, &event.xbutton.y,
                  &event.xbutton.state);
    event.xbutton.subwindow = event.xbutton.window;
    while (event.xbutton.subwindow) {
        event.xbutton.window = event.xbutton.subwindow;
        XQueryPointer(display, event.xbutton.window,
                      &event.xbutton.root, &event.xbutton.subwindow,
                      &event.xbutton.x_root, &event.xbutton.y_root,
                      &event.xbutton.x, &event.xbutton.y,
                      &event.xbutton.state);
    }
    event.xbutton.state |= Button1Mask;
    XSendEvent(display, PointerWindow, True, 0xfff, &event);
    XFlush(display);

    event.type = ButtonRelease;
    event.xbutton.state &= ~Button1Mask;
    XSendEvent(display, PointerWindow, True, 0xfff, &event);
    XFlush(display);

    XCloseDisplay(display);
}

void listenForKeyPress() {
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        std::cerr << "Error: Could not open display." << std::endl;
        return;
    }

    // Captura a tecla Insert
    XGrabKey(display, XKeysymToKeycode(display, XK_Insert), AnyModifier, DefaultRootWindow(display), True, GrabModeAsync, GrabModeAsync);

    XEvent event;
    while (running) {
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            if (event.xkey.keycode == XKeysymToKeycode(display, XK_Insert)) {
                paused = !paused; // Alterna entre pausado e despausado
            }
        }
    }

    XCloseDisplay(display);
}

void printStatus() {
    initscr(); // Inicializa a tela do ncurses
    start_color(); // Inicializa o suporte a cores
    init_pair(1, COLOR_WHITE, COLOR_BLACK); // Cinza (usando branco como substituto)
    init_pair(2, COLOR_RED, COLOR_BLACK); // Vermelho
    init_pair(3, COLOR_BLUE, COLOR_BLACK); // Azul
    init_pair(4, COLOR_GREEN, COLOR_BLACK); // Verde

    while (running) {
        clear(); // Limpa a tela

        // Captura a saída do FIGlet
        FILE *pipe = popen("figlet -f slant KillDarkness", "r");
        if (!pipe) {
            printw("Erro ao executar FIGlet.\n");
            refresh();
            continue;
        }

        // Exibe o ASCII art "KillDarkness"
        attron(COLOR_PAIR(1));
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            printw("%s", buffer); // Exibe cada linha do FIGlet
        }
        attroff(COLOR_PAIR(1));

        pclose(pipe); // Fecha o pipe

        // Exibe a mensagem de status
        if (paused) {
            attron(COLOR_PAIR(2));
            printw("\nO AutoClicker está desativado! ");
            attroff(COLOR_PAIR(2));

            attron(COLOR_PAIR(3) | A_BOLD); // Azul e negrito
            printw("Insert");
            attroff(COLOR_PAIR(3) | A_BOLD);

            attron(COLOR_PAIR(2));
            printw(" para ativa-lo.\n");
            attroff(COLOR_PAIR(2));
        } else {
            attron(COLOR_PAIR(4));
            printw("\nO AutoClicker está ativado! ");
            attroff(COLOR_PAIR(4));

            attron(COLOR_PAIR(3) | A_BOLD); // Azul e negrito
            printw("Insert");
            attroff(COLOR_PAIR(3) | A_BOLD);

            attron(COLOR_PAIR(4));
            printw(" para desativa-lo.\n");
            attroff(COLOR_PAIR(4));
        }

        refresh(); // Atualiza a tela
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Evita uso excessivo da CPU
    }

    endwin(); // Finaliza o ncurses
}

int main() {
    std::thread keyListener(listenForKeyPress); // Thread para escutar a tecla Insert
    std::thread statusPrinter(printStatus); // Thread para exibir o status

    while (running) {
        if (!paused) {
            clickMouse();
            std::this_thread::sleep_for(std::chrono::microseconds(1)); // Ajuste o tempo entre cliques
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Evita uso excessivo da CPU quando pausado
        }
    }

    keyListener.join(); // Aguarda a thread do listener terminar
    statusPrinter.join(); // Aguarda a thread do status terminar
    return 0;
}
