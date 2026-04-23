class Program
{
    static async Task Main()
    {
        Console.Write( "Введите путь к файлу: " );
        string path = Console.ReadLine();

        if ( !File.Exists( path ) )
        {
            Console.WriteLine( "Файл не найден!" );
            return;
        }

        Console.Write( "Введите символы для удаления (например: !?,.): " );
        string charsToRemove = Console.ReadLine();

        string text = await File.ReadAllTextAsync( path );

        // удаляем символы
        foreach ( char c in charsToRemove )
            text = text.Replace( c.ToString(), "" );

        // сохраняем асинхронно
        await File.WriteAllTextAsync( path, text );

        Console.WriteLine( "Готово! Файл сохранён." );
    }
}
