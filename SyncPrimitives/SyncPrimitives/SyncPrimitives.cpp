#include <windows.h>
#include <string>
#include <iostream>
#include <fstream>
#include "tchar.h"

CRITICAL_SECTION FileCriticalSection;
HANDLE hMutex = NULL;

bool useMutex = false;

void Lock()
{
    if (useMutex)
        WaitForSingleObject(hMutex, INFINITE);
    else
        EnterCriticalSection(&FileCriticalSection);
}

void Unlock()
{
    if (useMutex)
        ReleaseMutex(hMutex);
    else
        LeaveCriticalSection(&FileCriticalSection);
}

int ReadFromFile()
{
    std::fstream myfile("balance.txt", std::ios_base::in);
    int result = 0;
    myfile >> result;
    return result;
}

void WriteToFile(int data)
{
    std::fstream myfile("balance.txt", std::ios_base::out);
    myfile << data << std::endl;
}

void Deposit(int money)
{
    Lock();
    int balance = ReadFromFile();
    balance += money;
    WriteToFile(balance);
    std::cout << "Deposit: +" << money << ", balance = " << balance << "\n";
    Unlock();
}

void Withdraw(int money)
{
    Lock();
    int balance = ReadFromFile();

    if (balance < money)
    {
        std::cout << "Withdraw (" << money << ") failed, balance = " << balance << "\n";
        Unlock();
        return;
    }

    Sleep(20);
    balance -= money;
    WriteToFile(balance);
    std::cout << "Withdraw: -" << money << ", balance = " << balance << "\n";
    Unlock();
}

DWORD WINAPI ThreadDeposit(LPVOID param)
{
    Deposit((int)param);
    return 0;
}

DWORD WINAPI ThreadWithdraw(LPVOID param)
{
    Withdraw((int)param);
    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc > 1)
    {
        if (_tcscmp(argv[1], _T("mutex")) == 0)
        {
            useMutex = true;
        }
    }

    if (useMutex)
    {
        hMutex = CreateMutex(NULL, FALSE, _T("Global\\BalanceMutex"));
        if (!hMutex)
        {
            std::cerr << "Mutex creation failed.\n";
            return 1;
        }
        std::cout << "[MODE] Using MUTEX\n";
    }
    else
    {
        InitializeCriticalSection(&FileCriticalSection);
        std::cout << "[MODE] Using CRITICAL_SECTION\n";
    }

    WriteToFile(0);

    HANDLE handles[50];

    SetProcessAffinityMask(GetCurrentProcess(), 1);

    for (int i = 0; i < 50; i++)
    {
        handles[i] = (i % 2 == 0)
            ? CreateThread(NULL, 0, ThreadDeposit, (LPVOID)230, 0, NULL)
            : CreateThread(NULL, 0, ThreadWithdraw, (LPVOID)1000, 0, NULL);
    }

    WaitForMultipleObjects(50, handles, TRUE, INFINITE);

    int finalBalance = ReadFromFile();
    std::cout << "Final Balance = " << finalBalance << "\n";

    if (!useMutex)
        DeleteCriticalSection(&FileCriticalSection);
    else
        CloseHandle(hMutex);

    system("pause");
    return 0;
}
