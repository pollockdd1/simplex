#ifndef IO_h_
#define IO_h_

#include <fstream>
#include <string>
#include <map>

enum class IOtype {
	INPUT,
	OUTPUT
};

struct fileInfo {
	std::string path;
	std::string file_name;
	IOtype t;
};

namespace IO {
class Files {
public:
	Files();
	void setupOutput();
	void add_file(std::string name, std::string path, IOtype);
	std::ifstream get_ifstream(std::string name);
	std::ofstream get_ofstream(std::string name);
	void print();
	void close();

private:
	int total_files;
	std::map<std::string, int> file_to_index;
	std::map<int, fileInfo> file_values;

	std::string defaultfile;
	std::string optionsfile;
	std::string outdir;

	void check_stream(const std::string&, const std::string&, std::ifstream&);
	std::string filename_from_path(std::string);
	void copyFile(const std::string&, const std::string&);
	inline std::string findFullFilePath(std::string parameter);  
	void ConfigureOutputDirectory();
};
}

#endif
