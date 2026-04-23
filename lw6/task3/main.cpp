#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <omp.h>

using SquareMatrix = std::vector<std::vector<double>>;

static SquareMatrix ReadMatrix(std::istream& input)
{
    SquareMatrix matrix;
    std::string line;

    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }

        std::vector<double> row;
        std::istringstream iss(line);
        double value;

        while (iss >> value)
        {
            row.push_back(value);
        }

        matrix.push_back(row);
    }

    return matrix;
}

static void PrintMatrix(std::ostream& output, const SquareMatrix& matrix)
{
    for (int i = 0; i < matrix.size(); i++)
    {
        for (int j = 0; j < matrix[i].size(); j++)
        {
            output << matrix[i][j] << "\t";
        }
        output << std::endl;
    }
}

static SquareMatrix MultiplyMatrix(const SquareMatrix& first, const SquareMatrix& second)
{
    size_t n = first.size();
    SquareMatrix result(n, std::vector<double>(n, 0.0));

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            double sum = 0.0;
            for (int k = 0; k < n; k++)
            {
                sum += first[i][k] * second[k][j];
            }
            result[i][j] = sum;
        }
    }

    return result;
}


int main()
{
    std::ifstream matrixA;
    matrixA.open("matrixA.txt");

    std::ifstream matrixB;
    matrixB.open("matrixB.txt");

    if (!matrixA.is_open() || !matrixB.is_open())
    {
        std::cout << "Can`t open input files!\n";
        return EXIT_FAILURE;
    }

    SquareMatrix matrix1 = ReadMatrix(matrixA);
    SquareMatrix matrix2 = ReadMatrix(matrixB);

    if (matrix1.size() != matrix2.size() ||
        matrix1[0].size() != matrix2.size())
    {
        std::cerr << "Error: Incompatible matrix dimensions for multiplication!" << std::endl;
        return EXIT_FAILURE;
    }

    SquareMatrix product = MultiplyMatrix(matrix1, matrix2);

    std::cout << "Result of multiplication:" << std::endl;
    PrintMatrix(std::cout, product);

    return EXIT_SUCCESS;
}