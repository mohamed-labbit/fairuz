#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>


using namespace std;
namespace fs = filesystem;


int main()
{
    string      directories[] = {"/Users/mohamedrabbit/mylang/src/",
                                 "/Users/mohamedrabbit/mylang/include/"};
    std::size_t total_lines   = 0;
    std::size_t num_of_files  = 0;

    vector<pair<fs::path, std::size_t>> file_line_counts;

    for (auto directory : directories)
    {
        try
        {
            if (!fs::is_directory(directory))
            {
                cerr << "Error: " << directory << " is not a valid directory." << endl;
                return 1;
            }

            for (const auto& entry : fs::recursive_directory_iterator(directory))
            {
                if (entry.is_regular_file())
                {
                    ifstream file(entry.path());
                    if (!file.is_open())
                    {
                        cerr << "Warning: Could not open file: " << entry.path() << endl;
                        continue;
                    }

                    string line;
                    auto   sub_count = 0;

                    while (getline(file, line))
                    {
                        sub_count++;
                    }

                    total_lines += sub_count;
                    file_line_counts.emplace_back(entry.path(), sub_count);
                    num_of_files++;
                }
            }
        } catch (const fs::filesystem_error& e)
        {
            cerr << "Filesystem error: " << e.what() << endl;
            return 1;
        } catch (const exception& e)
        {
            cerr << "Error: " << e.what() << endl;
            return 1;
        }
    }

    // Sort by line count in ascending order
    sort(file_line_counts.begin(), file_line_counts.end(),
         [](const auto& a, const auto& b) { return a.second < b.second; });

    // Print sorted results
    for (const auto& [path, lines] : file_line_counts)
    {
        cout << "File processed : " << path << " | lines = " << lines << '\n';
    }

    cout << "Total lines: " << total_lines << endl;
    cout << "Total files: " << num_of_files << endl;

    return 0;
}