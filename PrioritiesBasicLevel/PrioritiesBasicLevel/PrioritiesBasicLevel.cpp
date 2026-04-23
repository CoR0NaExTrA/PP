#include <windows.h>
#include <iostream>
#include <tchar.h>
#include <vector>
#include <fstream>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// Глобальные переменные для синхронизации доступа к файлу
HANDLE g_hFileMutex;
std::ofstream g_outputFile;

// Структура для передачи параметров в поток
struct ThreadParams {
    int threadNumber;
    int operationsCount;
    DWORD startTime;
};

DWORD WINAPI ThreadProc(CONST LPVOID lpParam)
{
    ThreadParams* params = (ThreadParams*)lpParam;
    int threadNum = params->threadNumber;
    int operationsCount = params->operationsCount;
    DWORD startTime = params->startTime;

    for (int i = 0; i < operationsCount; i++) {
        // Имитация работы - вычисление простых чисел
        volatile int primeCheck = 0;
        for (int j = 2; j < 10000; j++) {
            primeCheck = 0;
            for (int k = 2; k * k <= j; k++) {
                if (j % k == 0) {
                    primeCheck = 1;
                    break;
                }
            }
        }

        // текущее время
        DWORD currentTime = timeGetTime() - startTime;

        // доступ к файлу
        WaitForSingleObject(g_hFileMutex, INFINITE);

        // Записываем в файл
        g_outputFile << threadNum << "|" << currentTime << std::endl;

        // Освобождаем мьютекс
        ReleaseMutex(g_hFileMutex);
    }

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    setlocale(LC_ALL, "RU");

    if (argc < 2) {
        std::cout << "Использование: program.exe <N>" << std::endl;
        std::cout << "N - количество операций в каждом потоке" << std::endl;
        return 1;
    }

    int operationsCount = _ttoi(argv[1]);

    if (operationsCount <= 0) {
        std::cout << "Количество операций должно быть больше 0!" << std::endl;
        return 1;
    }

    // мьютекс для синхронизации доступа к файлу
    g_hFileMutex = CreateMutex(NULL, FALSE, NULL);
    if (g_hFileMutex == NULL) {
        std::cerr << "Ошибка создания мьютекса" << std::endl;
        return 1;
    }

    g_outputFile.open("thread_times.csv");
    if (!g_outputFile.is_open()) {
        std::cerr << "Ошибка открытия файла для записи" << std::endl;
        CloseHandle(g_hFileMutex);
        return 1;
    }

    g_outputFile << "ThreadID|TimeMs" << std::endl;

    std::cout << "Программа запущена" << std::endl;
    std::cin.get();

    // Фиксируем время начала работы
    DWORD startTime = timeGetTime();

    // Создаем параметры для потоков
    ThreadParams params1 = { 1, operationsCount, startTime };
    ThreadParams params2 = { 2, operationsCount, startTime };

    // Создаем два потока
    HANDLE handles[2];

    handles[0] = CreateThread(
        NULL,
        0,
        &ThreadProc,
        &params1,
        0,
        NULL
    );

    handles[1] = CreateThread(
        NULL,
        0,
        &ThreadProc,
        &params2,
        0,
        NULL
    );

    // Проверка создания потоков
    if (handles[0] == NULL || handles[1] == NULL) {
        std::cerr << "Ошибка создания потоков" << std::endl;
        g_outputFile.close();
        CloseHandle(g_hFileMutex);
        return 1;
    }

    WaitForMultipleObjects(2, handles, TRUE, INFINITE);

    // Закрываем ресурсы
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);
    g_outputFile.close();
    CloseHandle(g_hFileMutex);

    std::cout << "Программа завершена" << std::endl;
    std::cin.get();

    return 0;
}