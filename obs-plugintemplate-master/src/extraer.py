import os

def extraer_codigo_a_texto(directorio_origen, archivo_destino):
    # Definimos las extensiones que queremos extraer
    extensiones_validas = ('.c', '.cpp', '.h')
    
    archivos_procesados = 0

    # Abrimos el archivo de destino en modo escritura
    with open(archivo_destino, 'w', encoding='utf-8') as out_file:
        out_file.write("# RECOPILACIÓN DE CÓDIGO FUENTE\n\n")
        
        # os.walk recorre la carpeta actual y todas las subcarpetas
        for directorio_raiz, _, archivos in os.walk(directorio_origen):
            for nombre_archivo in archivos:
                # Comprobamos si termina en .c, .cpp o .h
                if nombre_archivo.endswith(extensiones_validas):
                    ruta_completa = os.path.join(directorio_raiz, nombre_archivo)
                    
                    # Escribimos un encabezado claro para separar cada archivo
                    out_file.write(f"\n{'='*60}\n")
                    out_file.write(f"/// ARCHIVO: {ruta_completa} ///\n")
                    out_file.write(f"{'='*60}\n\n")
                    
                    # Intentamos leer el archivo y añadirlo al destino
                    try:
                        # Usamos errors='ignore' por si hay algún carácter extraño que no sea UTF-8
                        with open(ruta_completa, 'r', encoding='utf-8', errors='ignore') as in_file:
                            contenido = in_file.read()
                            out_file.write(contenido)
                            out_file.write("\n")
                            archivos_procesados += 1
                    except Exception as e:
                        out_file.write(f"// [ERROR AL LEER ESTE ARCHIVO: {e}]\n\n")

    print(f"✅ ¡Proceso completado!")
    print(f"📁 Se han procesado {archivos_procesados} archivos.")
    print(f"📄 Todo el código se ha guardado en: {archivo_destino}")

if __name__ == "__main__":
    # CONFIGURACIÓN
    # '.' significa que buscará en la misma carpeta donde ejecutes este script
    CARPETA_A_ANALIZAR = '.' 
    
    # Nombre del archivo que generará (este es el que le pasarás a antigravity)
    ARCHIVO_RESULTANTE = 'todo_mi_codigo.txt' 
    
    extraer_codigo_a_texto(CARPETA_A_ANALIZAR, ARCHIVO_RESULTANTE)