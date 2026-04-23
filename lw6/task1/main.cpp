#include <iostream>
#include <omp.h>

static double GregoryLeibniz_Sequential(int terms)
{
    double pi_approx = 0.0;
    for (int i = 0; i < terms; ++i) 
    {
        pi_approx += (i % 2 == 0 ? 1.0 : -1.0) / (2.0 * i + 1.0);
    }

    return pi_approx * 4.0;
}

static double GregoryLeibniz_BrokenParallel(int terms)
{
    double pi_approx = 0.0;

    #pragma omp parallel for
    for (int i = 0; i < terms; ++i) 
    {
        double term = (i % 2 == 0 ? 1.0 : -1.0) / (2.0 * i + 1.0);
        pi_approx += term; 
    }

    return pi_approx * 4.0;
}

static double GregoryLeibniz_Atomic(int terms)
{
    double pi_approx = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < terms; ++i) 
    {
        double term = (i % 2 == 0 ? 1.0 : -1.0) / (2.0 * i + 1.0);

        #pragma omp atomic
        pi_approx += term;
    }

    return pi_approx * 4.0;
}

static double GregoryLeibniz_Reduction(int terms)
{
    double pi_approx = 0.0;

    #pragma omp parallel for reduction(+:pi_approx)
    for (int i = 0; i < terms; ++i) 
    {
        pi_approx += (i % 2 == 0 ? 1.0 : -1.0) / (2.0 * i + 1.0);
    }

    return pi_approx * 4.0;
}

int main() 
{
    const int num_terms = 100'000'000;

    std::cout.precision(12);

    auto test_version = [&](const char* name, double (*func)(int)) 
    {
        double start = omp_get_wtime();
        double pi = func(num_terms);
        double elapsed = omp_get_wtime() - start;

        std::cout << name << ":\n  Pi = " << pi
            << "\n  Time: " << elapsed << "s\n\n";
    };

    test_version("Sequential", GregoryLeibniz_Sequential);
    test_version("Broken Parallel", GregoryLeibniz_BrokenParallel);
    test_version("Atomic Version", GregoryLeibniz_Atomic);
    test_version("Reduction Version", GregoryLeibniz_Reduction);

    return EXIT_SUCCESS;
}