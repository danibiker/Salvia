#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <io/fileio.h>

class FileList {
public:
    // Método para guardar el vector en un archivo
    static bool guardarVector(const std::string& nombreArchivo, const std::vector<std::string>& datos) {
        // Se abre el flujo de salida hacia el archivo
        std::ofstream archivo(nombreArchivo);

        // Verificar si el archivo se abrió correctamente
        if (!archivo.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo " << nombreArchivo << std::endl;
            return false;
        }

        // Iterar sobre el vector y escribir cada string en una línea nueva
        //for (const std::string& linea : datos) {
		for (unsigned int i=0; i < datos.size(); i++){
		    archivo << datos.at(i) << "\n";
        }
        archivo.close();
		Fileio::commit(nombreArchivo.c_str());
        return true;
    }

	// Carga las líneas de un archivo en un vector de strings
    static bool cargarVector(const std::string& nombreArchivo, std::vector<std::string>& datos) {
        std::ifstream archivo(nombreArchivo);
        std::string linea;

        if (!archivo.is_open()) {
            return false;
        }

        // Limpiamos el vector antes de cargar nuevos datos
        datos.clear();

        // Leemos línea por línea hasta el final del archivo
        while (std::getline(archivo, linea)) {
            datos.push_back(linea);
        }

        archivo.close();
        return true;
    }
};