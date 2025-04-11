#include <iostream>
#include <thread>
#include <chrono>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <atomic>
#include <ncurses.h>
#include <cstdio> // Para popen e pclose
#include <cstring> // Para strcpy e strcat
#include <random>  // Para geração de números aleatórios

std::atomic<bool> paused(true); // Inicialmente pausado
std::atomic<bool> running(true); // Controla o loop principal
int minCPS = 10;  // CPS mínimo padrão
int maxCPS = 15;  // CPS máximo padrão

// Gerador de números aleatórios
std::random_device rd;
std::mt19937 gen(rd());

// Códigos ANSI para cores no terminal
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string BLUE = "\033[34m";
const std::string CYAN = "\033[36m";
const std::string YELLOW = "\033[33m";
const std::string BOLD = "\033[1m";

// Exibe o ASCII art KillDarkness no terminal normal
void showAsciiTitle() {
    // Limpa o terminal
    system("clear");
    
    // Executa o figlet para gerar o ASCII art
    FILE *pipe = popen("figlet -f slant KillDarkness", "r");
    if (!pipe) {
        std::cerr << "Erro ao executar FIGlet." << std::endl;
        return;
    }
    
    // Exibe o ASCII art com cor
    std::cout << CYAN << BOLD;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::cout << buffer;
    }
    std::cout << RESET << std::endl;
    
    pclose(pipe);
    
    // Informações sobre o programa
    std::cout << BLUE << "Responda o min e max cps abaixo:" << RESET << std::endl << std::endl;
}

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
    init_pair(1, COLOR_CYAN, COLOR_BLACK); // Ciano
    init_pair(2, COLOR_RED, COLOR_BLACK); // Vermelho
    init_pair(3, COLOR_BLUE, COLOR_BLACK); // Azul
    init_pair(4, COLOR_GREEN, COLOR_BLACK); // Verde
    init_pair(5, COLOR_YELLOW, COLOR_BLACK); // Amarelo

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
        attron(COLOR_PAIR(1) | A_BOLD);
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            printw("%s", buffer); // Exibe cada linha do FIGlet
        }
        attroff(COLOR_PAIR(1) | A_BOLD);

        pclose(pipe); // Fecha o pipe

        // Exibe a configuração de CPS
        attron(COLOR_PAIR(5));
        printw("\nConfiguração: %d-%d CPS (Clicks Por Segundo)\n", minCPS, maxCPS);
        attroff(COLOR_PAIR(5));

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

// Função para obter um atraso aleatório baseado no CPS
int getRandomDelay() {
    std::uniform_int_distribution<> cpsDistrib(minCPS, maxCPS);
    int currentCPS = cpsDistrib(gen);
    // Convertendo CPS para microssegundos (1 segundo = 1000000 microssegundos)
    return 1000000 / currentCPS;
}

int main() {
    // Exibe o título ASCII colorido e limpa a tela
    showAsciiTitle();
    
    // Perguntar pelo CPS mínimo e máximo com cores
    std::cout << GREEN << "Digite o CPS mínimo: " << RESET;
    std::cin >> minCPS;
    
    std::cout << GREEN << "Digite o CPS máximo: " << RESET;
    std::cin >> maxCPS;
    
    // Verificação básica dos valores
    if (minCPS <= 0 || maxCPS <= 0 || minCPS > maxCPS) {
        std::cout << RED << "Valores inválidos! Usando padrões: 10-15 CPS" << RESET << std::endl;
        minCPS = 10;
        maxCPS = 15;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Pausa para mostrar a mensagem
    }
    
    std::cout << BLUE << "Iniciando AutoClicker com " << BOLD << minCPS << "-" << maxCPS << " CPS..." << RESET << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Pausa para mostrar a mensagem

    std::thread keyListener(listenForKeyPress); // Thread para escutar a tecla Insert
    std::thread statusPrinter(printStatus); // Thread para exibir o status

    while (running) {
        if (!paused) {
            clickMouse();
            // Usar um atraso aleatório baseado no intervalo de CPS configurado
            int delayMicros = getRandomDelay();
            std::this_thread::sleep_for(std::chrono::microseconds(delayMicros));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Evita uso excessivo da CPU quando pausado
        }
    }

    keyListener.join(); // Aguarda a thread do listener terminar
    statusPrinter.join(); // Aguarda a thread do status terminar
    return 0;
}