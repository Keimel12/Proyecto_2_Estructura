#include <iostream>
#include <algorithm>
#include <include/curl/curl.h>
#include <cstddef>
#include <queue>
#include <string>
#include <fstream>
#include <set>
#include <vector>

/*******************************************************************************************************
*   write_callback(char *, size_t, void *): Se encarga de procesar los datos de la descarga de la pagina
*   web.
********************************************************************************************************/
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    // Convertimos el puntero void* a una cadena
    std::string* response = reinterpret_cast<std::string*>(userdata);

    // Agregamos los datos recibidos al final de la cadena
    response->append(ptr, size * nmemb);

    // Retornamos el tamaño total de datos recibidos
    return size * nmemb;
}

// Capa de logica de negocio
class GestorURL {
private:
    // Cola donde se almacenaran los enlaces
    std::queue<std::string> enlaces;
    // set que evitara la insercion de enlaces repetidos en la cola
    std::set<std::string> enlacesUnicos;

    /*******************************************************************************************************
    *  	get_dominio(const std::string&): Se encarga de retornar el	dominio de la url a consultar usando una
    *   subcadena
    ********************************************************************************************************/
    std::string get_dominio(const std::string& url) {
            // Obtenemos la posicion de "://" (perteneciente normalmente a "https://"
            size_t start = url.find("://");

            // En caso de no encontrar similitud, comenzamos en 0
            if (start == std::string::npos) {
                start = 0;
            } else {
                // De lo contrario, empezaremos la subcadena desde el primer caracter del dominio
                start += 3;
            }

            /*  Obtenemos la posicion del ultimo caracter perteneciente del url (es usada en caso
            de haber mas texto luego del dominio, que normalmente viene al final de un '/').    */
            size_t longitud = url.find('/', start);

            // En caso de no haber mas elementos luego del dominio, el final sera en la longitud total del url
            if (longitud == std::string::npos) {
                longitud = url.length();
            }

            // Procedemos a obtener la subcadena correspondiente al dominio de la url
            std::string dominio = url.substr(start, longitud - start);

            // En caso de poseer "https://" o "www." procedemos a obtener la subcadena eliminando dicha parte
            if (dominio.find("https://") == 0) {
                dominio = dominio.substr(8);
            } else if (dominio.find("www.") == 0) {
                dominio = dominio.substr(4);
            }

            // retornamos el dominio
            return dominio;
        }

public:
    /********************************************************************************************************
    *   header_callback(char *, size_t, size_t, void *): Procesamos los encabezados de las respuestas HTTP al
    *   descargar la pagina web
    *********************************************************************************************************/
    size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
        // Creamos una cadena a partir de los encabezados de las respuestas HTTP
        std::string headerLine(buffer, size * nitems);

        // Nos ubicamos en la cadena "Location:"
        size_t pos = headerLine.find("Location:");

        // En caso de encontrar la cadena
        if (pos != std::string::npos && pos == 0) {
            // Extraemos la cadena de redireccion
            std::string redirectUrl = headerLine.substr(10);
            redirectUrl.erase(std::remove_if(redirectUrl.begin(), redirectUrl.end(), ::isspace), redirectUrl.end());
            // Establecemos la redireccion de la url
            curl_easy_setopt((CURL *)userdata, CURLOPT_URL, redirectUrl.c_str());
        }
        return size * nitems;
    }

    // Función que realiza la petición HTTP y retorna el código HTML de la página web.
    std::string get_html(const std::string& url) {
        // Procedemos a inicializar el curl
        CURL* curl = curl_easy_init();

        if (curl) {
            CURLcode res;
            std::string response;
            // Establecemos el url de la pagina a descargar
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Establecemos que pueda seguir las redirecciones
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Establecemos que procesaremos los datos recibidos
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            // Descargamos la pagina web indicada por la url
            res = curl_easy_perform(curl);

            // Liberamos los recursos usados por los metodos de la libreria curl
            curl_easy_cleanup(curl);

            // En caso de que no haya errores en la descarga de la pagina
            if (res == CURLE_OK) {
                // Procedemos a retornar el html
                return response;
            } else {
                // De lo contrario notificamos que existio un error
                std::cerr << "Error al descargar la pagina: " << curl_easy_strerror(res) << std::endl;
            }
        }
        return "";
    }

    /***************************************************************************************
    *   substrEnlaceHttps(std::string &): Retorna el url eliminando el "prefijo": "https://
    ***************************************************************************************/
    std::string substrEnlaceHttps(std::string &url) {
        // Nos ubicamos en el primer caracter del dominio
        int inicio = url.find("//") + 2;
        // Nos ubicamos en despues del ultimo caracter del dominio
        int limite = url.find("/", inicio) - inicio;

        // Obtenemos una subcadena a partir de los valores en 'inicio' y 'limite' adicionando "/"
        url = url.substr(inicio, limite) + "/";

        // Retornamos la url
        return url;
    }

    /**********************************************************************************
    *   substrEnlaceWWW(std::string &): Retorna el url eliminando el "prefijo": "www."
    **********************************************************************************/
    std::string substrEnlaceWWW(std::string &url) {
        // En caso de tener la cadena "https://" nos ubicamos al siguiente caracter de esta
        int inicio = url.find("://") + 3;
        // En caso de que posea tambien la cadena "www." nos ubicamos en el primer caracter del dominio
        if (url.find("www.") == inicio) {
            inicio += 4;
        }

        // Nos ubicamos en despues del ultimo caracter del dominio
        int limite = url.find("/", inicio);

        // Obtenemos una subcadena a partir de los valores en 'inicio' y 'limite' adicionando "/"
        std::string domain = url.substr(inicio, limite - inicio);

        // En caso de no hallar la cadena "www." al inicio de la url, procedemos a eliminarla
        if (domain.find("www.") == 0) {
            domain = domain.substr(4);
        }

        // Retornamos el dominio
        return domain;
    }

    /*****************************************************************************************
    *   get_links(const std::string &); Se encarga de insertar los enlaces dentro de la cola
    *****************************************************************************************/
    void get_links(const std::string& url) {
        std::string enlace = url;

        // En caso de tener una cadena "www." procedemos a eliminarla
        if(url.find("www.") != std::string::npos) {
            substrEnlaceWWW(enlace);
        }
        // En caso de tener una cadena "https://" procedemos a eliminarla
        if(url.find("https://") != std::string::npos) {
            substrEnlaceHttps(enlace);
        }

        // Procedemos a almacenar el html
        std::string html = get_html(enlace);

        // Si html no esta vacio
        if (!html.empty()) {
            // Procedemos a almacenar el dominio
            std::string dominio = get_dominio(enlace);;

            size_t start = 0;

            // Mientras que se pueda encontrar la etiqueta href (enlace)
            while ((start = html.find("href=", start)) != std::string::npos) {
                start += 6;
                // Nos ubicamos al final del enlace
                size_t limite = html.find_first_of("\"'", start);

                // Si existe el final de dicho enlace de la manera especificada
                if (limite != std::string::npos) {
                    // Almacenamos el link a partir de la subcadena
                    std::string link = html.substr(start, limite - start);

                    // Si existe el dominio en el link procedemos a insertar el enlace en el std::set
                    if(link.find(dominio)!= std::string::npos) {
                         // Si al insertar en el set, su booleano (.second) da true, entonces...
                        if(enlacesUnicos.insert(link).second){
                            // Insertamos el elemento en la cola (para evitar enlaces repetidos)
                            enlaces.push(link);
                        }
                    }
                    start = limite + 1;
                } else {
                    // De lo contrario, se rompe el ciclo
                    break;
                }
            }
        }
        // Limpiamos el set
        enlacesUnicos.clear();
    }

    /*******************************************************************************
    *   display(): Retorna el primer elemento de nuestro atributo y luego lo elimina
    ********************************************************************************/
    std::string display() {
        std::string url = "";
        // Si el std::set no esta vacio
        if(!enlaces.empty()) {
            // url obtendra el valor de la primer enlace
            url = enlaces.front();

            // Eliminamos el primer enlace
            enlaces.pop();
        }
        // Retornamos la url
        return url;
    }

    /**************************************************************************************************
    *   empty(): Retorna 'true' en caso de que el std::set este vacio, de lo contrario, retorna 'false'
    **************************************************************************************************/
    bool empty() {
        if(!enlaces.empty()) {
            return true;
        } else {
            return false;
        }
    }
};

// Capa de datos
class GestorArchivosOfstream {
private:
    std::string nombreArchivo;

public:
    /********************************************************************************
    *   setNombreArchivo(std::string): Permite cambiar el nombre de archivo a acceder
    *********************************************************************************/
    void setNombreArchivo(std::string nombreArchivo) {
        this->nombreArchivo = nombreArchivo;
    }

    /***************************************************************
    *   getNombreArchivo(std::string): Retorna el nombre del archivo
    ****************************************************************/
    std::string getNombreArchivo() {
        return nombreArchivo;
    }

    /********************************************************
    *   anadirURL(std::string): Añade una url al archivo .txt
    ********************************************************/
    void anadirURL(std::string url) {
        std::ofstream archivo(nombreArchivo, std::ios_base::app);

        archivo << url << std::endl;

        archivo.close();
    }

    /**************************************************************************************
    *   borrarDatos(): Permite truncar el archivo txt para borrar los datos en su interior
    **************************************************************************************/
    void borrarDatos() {
        std::ofstream archivo;

        archivo.open(nombreArchivo, std::ofstream::out | std::ofstream::trunc);

        archivo.close();
    }
};

// Capa de datos
class GestorArchivosIfstream {
private:
    std::string nombreArchivo;

public:

    /********************************************************************************
    *   leerURL(): Muestra en el terminal todo el contenido que posee el archivo .txt
    *********************************************************************************/
    void leerURL() {
        std::ifstream archivo(nombreArchivo);
        std::string linea;

        if(archivo.is_open()) {
            while(std::getline(archivo, linea)) {
                std::cout << linea << std::endl;
            }
            archivo.close();
        } else {
            std::cerr << "Error al abrir el archivo" << std::endl;
        }
    }
    /********************************************************************************
    *   setNombreArchivo(std::string): Permite cambiar el nombre de archivo a acceder
    *********************************************************************************/
    void setNombreArchivo(std::string nombreArchivo) {
        this->nombreArchivo = nombreArchivo;
    }

    /***************************************************************
    *   setNombreArchivo(std::string): Retorna el nombre del archivo
    ****************************************************************/
    std::string getNombreArchivo() {
        return nombreArchivo;
    }

};

// Capa de presentacion.
int main(int argc, char* argv[]) {
    // Iniciamos los gestores
    GestorURL gestor;
    GestorArchivosOfstream archivoEscritura;
    GestorArchivosIfstream archivoLectura;

    std::string enlace;
    std::string url;

    char opcion;

    // Establecemos las direcciones a donde accederan los archivos
    archivoEscritura.setNombreArchivo("enlacesURL.txt");
    archivoLectura.setNombreArchivo(archivoEscritura.getNombreArchivo());

    do
    {
        // Le pedimos al usuario que ingrese una opcion
        std::cout << "******************************************************************" << std::endl;
        std::cout << "1) Consultar enlaces pertenecientes a la pagina" << std::endl;
        std::cout << "2) Ver enlaces pertenecientes a la pagina" << std::endl;
        std::cout << "3) salir" << std::endl;
        std::cout << "******************************************************************" << std::endl;
        std::cout << "Escriba su opcion: ";
        std::cin >> opcion;
        std::cout << std::endl;

        switch(opcion) {
            case '1':
                // Borramos los datos que ya esten almacenados en el archivo
                archivoEscritura.borrarDatos();

                // Pedimos que ingrese la url
                std::cout << "Ingrese el url a buscar: ";
                std::cin >> enlace;
                gestor.get_links(enlace);

                // Añadimos los enlaces al archivo
                while(gestor.empty() == true) {
                    archivoEscritura.anadirURL(gestor.display());
                }
                break;
            case '2':
                // Leemos lo contenido en el archivo (enlaces)
                archivoLectura.leerURL();
                break;
            case '3':
                // Termina la ejecucion del programa
                std::cout << "Gracias por usar el programa" << std::endl;
                return 0;
        }

    } while(opcion != 3);
    return 0;
}
