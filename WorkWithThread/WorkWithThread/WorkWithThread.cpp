#include <windows.h>
#include <iostream>
#include <tchar.h>
#include <vector>

DWORD WINAPI ThreadProc(CONST LPVOID lpParam)
{
    int threadNum = *reinterpret_cast<int*>(lpParam);
    std::cout << "Поток №" << threadNum << " выполняет свою работу" << std::endl;

    ExitThread(0);
}

int _tmain(int argc, _TCHAR* argv[])
{

    setlocale(LC_ALL, "RU");

    if (argc < 2)
    {
        std::cout << "Использование: program.exe <N>" << std::endl;
        return 1;
    }

    int n = _ttoi(argv[1]);

    if (n <= 0)
    {
        std::cout << "Количество потоков должно быть больше 0!" << std::endl;
        return 1;
    }

    std::vector<HANDLE> handles(n);
    std::vector<int> threadNumbers(n);

    for (int i = 0; i < n; i++)
    {
        threadNumbers[i] = i + 1;

        handles[i] = CreateThread(
            NULL,                 
            0,                    
            &ThreadProc,       
            &i,
            0,                    
            NULL                  
        );

        if (handles[i] == NULL)
        {
            std::cerr << "Ошибка создания потока №" << i + 1 << std::endl;
        }
    }

    WaitForMultipleObjects(n, handles.data(), TRUE, INFINITE);
    return 0;
}
