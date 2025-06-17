

// Inclusión de la biblioteca Assimp (Open Asset Import Library) para la carga de modelos 3D.
#include <assimp/cimport.h> 
#include <assimp/scene.h> 
#include <assimp/postprocess.h> 
#include <assimp/material.h>

// Inclusión de las cabeceras del API de OBS Studio.
#include <obs-module.h> 
#include <graphics/graphics.h> 
#include <graphics/matrix4.h> 
#include <graphics/image-file.h>
#include <util/platform.h> 

// Inclusión de bibliotecas estándar de C.
#include <string.h> 
#include <assimp/types.h> 



/**
 * @brief Estructura para representar una malla (mesh) de un modelo 3D.
 *
 * Contiene los buffers de vértices e índices necesarios para el renderizado,
 * así como la información de la textura asociada.
 */
typedef struct {
	gs_vertbuffer_t *vb;  
	gs_indexbuffer_t *ib; 
	uint32_t num_indices;
	uint32_t num_vertex;  

	gs_image_file_t *
		image; 
	gs_texture_t *
		texture; 
} Mesh;


static gs_effect_t *effect = NULL;

/**
 * @brief Carga un efecto (shader) desde un archivo para su uso en el renderizado.
 * @param filename Nombre del archivo de efecto (.effect) a cargar.
 * @return `true` si el efecto se cargó correctamente, `false` en caso contrario.
 */
bool load_effect(const char *filename)
{
	// Obtiene la ruta completa al archivo de efecto dentro del directorio del módulo.
	char *effect_path = obs_module_file(filename);
	if (!effect_path) {
		blog(LOG_ERROR, "No se pudo construir la ruta del efecto: %s",
		     filename);
		return false;
	}


	obs_enter_graphics();
	// Crea el efecto a partir del archivo especificado.
	effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();

	// Libera la memoria de la ruta del archivo.
	bfree(effect_path);

	if (!effect) {
		blog(LOG_ERROR, "No se pudo cargar el efecto: %s", filename);
		return false;
	}

	return true;
}

// Puntero global a un array de mallas que componen el modelo cargado.
static Mesh *g_meshes = NULL;
// Contador del número de mallas en el array.
static size_t g_mesh_count = 0;
// Variable global para un vector de 4 componentes, inicializado a (1,1,1,1).
struct vec4 vec4_one = {1.0f, 1.0f, 1.0f, 1.0f};

/**
 * @brief Carga un modelo 3D desde un archivo utilizando Assimp.
 *
 * Procesa el archivo, extrae la información de las mallas (vértices, índices, texturas)
 * y crea los recursos gráficos correspondientes en OBS.
 *
 * @param path Ruta al archivo del modelo 3D.
 * @return `true` si el modelo se cargó correctamente, `false` en caso contrario.
 */
bool load_model_c(const char *path)
{
	// Importa el archivo del modelo usando Assimp.
	// `aiProcess_Triangulate`: Convierte todas las primitivas a triángulos.
	// `aiProcess_FlipUVs`: Invierte las coordenadas de textura en el eje Y.
	// `aiProcess_CalcTangentSpace`: Calcula las tangentes y bitangentes si no existen.
	const struct aiScene *scene =
		aiImportFile(path, aiProcess_Triangulate | aiProcess_FlipUVs |
					   aiProcess_CalcTangentSpace);

	// Verifica si la importación falló.
	if (!scene) {
		blog(LOG_ERROR, "Assimp error: %s", aiGetErrorString());
		return false;
	}

	// Si ya hay un modelo cargado (g_meshes no es nulo), se liberan sus recursos.
	if (g_meshes) {
		for (size_t i = 0; i < g_mesh_count; i++) {
			gs_vertexbuffer_destroy(g_meshes[i].vb);
			gs_indexbuffer_destroy(g_meshes[i].ib);
			// Libera los recursos de la imagen si existen.
			if (g_meshes[i].image) {
				gs_image_file_free(g_meshes[i].image);
				bfree(g_meshes[i].image);
			}
		}
		bfree(g_meshes);
		g_meshes = NULL;
		g_mesh_count = 0;
	}

	// Asigna memoria para el nuevo conjunto de mallas del modelo.
	g_mesh_count = scene->mNumMeshes;
	g_meshes = (Mesh *)bmalloc(sizeof(Mesh) * g_mesh_count);
	memset(g_meshes, 0, sizeof(Mesh) * g_mesh_count);

	// Itera sobre cada malla en la escena de Assimp.
	for (size_t m = 0; m < g_mesh_count; m++) {
		struct aiMesh *mesh = scene->mMeshes[m];
		size_t vert_count = mesh->mNumVertices;
		size_t idx_count = mesh->mNumFaces *
				   3; // Cada cara es un triángulo (3 índices).

		// Estructura para almacenar los datos de los vértices antes de crear el buffer de OBS.
		struct gs_vb_data *vbd =
			(struct gs_vb_data *)bmalloc(sizeof(struct gs_vb_data));
		memset(vbd, 0, sizeof(struct gs_vb_data));

		// Asigna memoria para los datos de los vértices.
		vbd->num = vert_count;
		vbd->points = (struct vec3 *)bmalloc(sizeof(struct vec3) *
						     vert_count);
		vbd->normals = (struct vec3 *)bmalloc(sizeof(struct vec3) *
						      vert_count);
		vbd->tangents = (struct vec3 *)bmalloc(sizeof(struct vec3) *
						       vert_count);
		vbd->colors =
			(uint32_t *)bmalloc(sizeof(uint32_t) * vert_count);

		// Copia los datos de cada vértice desde la estructura de Assimp a nuestra estructura.
		for (size_t i = 0; i < vert_count; i++) {
			// Posiciones
			vbd->points[i].x = mesh->mVertices[i].x;
			vbd->points[i].y = mesh->mVertices[i].y;
			vbd->points[i].z = mesh->mVertices[i].z;

			// Normales (si existen)
			if (mesh->mNormals) {
				vbd->normals[i].x = mesh->mNormals[i].x;
				vbd->normals[i].y = mesh->mNormals[i].y;
				vbd->normals[i].z = mesh->mNormals[i].z;
			} else {
				vbd->normals[i].x = 0.0f;
				vbd->normals[i].y = 0.0f;
				vbd->normals[i].z = 0.0f;
			}

			// Tangentes (si existen)
			if (mesh->mTangents) {
				vbd->tangents[i].x = mesh->mTangents[i].x;
				vbd->tangents[i].y = mesh->mTangents[i].y;
				vbd->tangents[i].z = mesh->mTangents[i].z;
			} else {
				vbd->tangents[i].x = 0.0f;
				vbd->tangents[i].y = 0.0f;
				vbd->tangents[i].z = 0.0f;
			}

			// Colores de vértice (si existen)
			if (mesh->mColors[0]) {
				uint8_t r = (uint8_t)(mesh->mColors[0][i].r *
						      255.0f);
				uint8_t g = (uint8_t)(mesh->mColors[0][i].g *
						      255.0f);
				uint8_t b = (uint8_t)(mesh->mColors[0][i].b *
						      255.0f);
				uint8_t a = (uint8_t)(mesh->mColors[0][i].a *
						      255.0f);
				// Empaqueta los componentes RGBA en un entero de 32 bits (formato AABBGGRR).
				vbd->colors[i] = (a << 24) | (b << 16) |
						 (g << 8) | r;
			} else {
				// Si no hay color de vértice, se usa blanco opaco por defecto.
				vbd->colors[i] = 0xFFFFFFFF;
			}
		}

		// Procesa las coordenadas de textura (UVs) si existen.
		if (mesh->mTextureCoords[0]) {
			vbd->num_tex = 1;
			vbd->tvarray = (struct gs_tvertarray *)bmalloc(
				sizeof(struct gs_tvertarray));
			vbd->tvarray[0].width = 2; // Coordenadas 2D (U, V).
			vbd->tvarray[0].array =
				bmalloc(sizeof(float) * 2 * vert_count);
			memset(vbd->tvarray[0].array, 0,
			       sizeof(float) * 2 * vert_count);

			const struct aiVector3D *uvs = mesh->mTextureCoords[0];
			float *uv_array = (float *)vbd->tvarray[0].array;

			// Copia las coordenadas UV.
			for (size_t i = 0; i < vert_count; i++) {
				uv_array[i * 2 + 0] = uvs[i].x;
				uv_array[i * 2 + 1] = uvs[i].y;
			}
		} else {
			vbd->num_tex = 0;
			vbd->tvarray = NULL;
		}

		// Asigna memoria y copia los índices de las caras.
		uint32_t *indices =
			(uint32_t *)bmalloc(sizeof(uint32_t) * idx_count);
		size_t idx_written = 0;
		for (size_t f = 0; f < mesh->mNumFaces; f++) {
			const struct aiFace *face = &mesh->mFaces[f];
			// Asegura que la cara es un triángulo.
			if (face->mNumIndices != 3)
				continue;
			// Copia los tres índices del triángulo.
			indices[idx_written++] = face->mIndices[0];
			indices[idx_written++] = face->mIndices[1];
			indices[idx_written++] = face->mIndices[2];
		}

		// Crea los buffers de la GPU dentro del contexto gráfico de OBS.
		obs_enter_graphics();
		// `gs_render_start(true)` no es usualmente necesario aquí, pero podría estar para asegurar un estado limpio.
		gs_render_start(true);

		// Crea el buffer de vértices y el buffer de índices.
		gs_vertbuffer_t *vb = gs_vertexbuffer_create(vbd, 0);
		gs_indexbuffer_t *ib = gs_indexbuffer_create(
			GS_UNSIGNED_LONG, indices, (uint32_t)idx_written, 0);

		obs_leave_graphics();

		// Almacena los punteros a los buffers y la información de la malla.
		g_meshes[m].vb = vb;
		g_meshes[m].ib = ib;
		g_meshes[m].num_indices = (uint32_t)idx_written;
		g_meshes[m].num_vertex = (uint32_t)vert_count;

		// Libera la memoria temporal usada para los datos de vértices e índices.
		bfree(indices);
		bfree(vbd->points);
		bfree(vbd->normals);
		bfree(vbd->tangents);
		bfree(vbd->colors);

		if (vbd->tvarray) {
			bfree(vbd->tvarray[0].array);
			bfree(vbd->tvarray);
		}
		bfree(vbd);

		// Procesa el material y la textura de la malla.
		struct aiMaterial *material =
			scene->mMaterials[mesh->mMaterialIndex];
		// Comprueba si el material tiene una textura de tipo difuso.
		if (material && aiGetMaterialTextureCount(
					material, aiTextureType_DIFFUSE) > 0) {
			struct aiString texPath;
			// Obtiene la ruta de la primera textura difusa.
			if (aiGetMaterialTexture(
				    material, aiTextureType_DIFFUSE, 0,
				    &texPath, NULL, NULL, NULL, NULL, NULL,
				    NULL) == AI_SUCCESS) {
				char fullTexPath[512] = {0};
				// Intenta construir la ruta completa de la textura, asumiendo que es relativa al archivo del modelo.
				const char *modelDir = strrchr(path, '/');
				if (!modelDir)
					modelDir = strrchr(path, '\\');

				if (modelDir) {
					size_t len = modelDir - path + 1;
					memcpy(fullTexPath, path, len);
					fullTexPath[len] = '\0';
					strncat(fullTexPath, texPath.data,
						sizeof(fullTexPath) - len - 1);
				} else {
					strncpy(fullTexPath, texPath.data,
						sizeof(fullTexPath) - 1);
					fullTexPath[sizeof(fullTexPath) - 1] =
						'\0';
				}

				// Carga la imagen usando las funciones de OBS.
				gs_image_file_t *image =
					(gs_image_file_t *)bmalloc(
						sizeof(gs_image_file_t));
				memset(image, 0, sizeof(gs_image_file_t));
				gs_image_file_init(image, fullTexPath);

				if (image->loaded) {
					blog(LOG_INFO, "Textura cargada: %s",
					     fullTexPath);
					g_meshes[m].image =
						image; // Guarda el objeto de imagen para gestión.
					g_meshes[m].texture =
						image->texture; // Guarda la textura para renderizar.
				} else {
					blog(LOG_WARNING,
					     "No se pudo cargar textura: %s",
					     fullTexPath);
					bfree(image); // Libera memoria si la carga falló.
				}
			}
		}
	}
	// Carga el efecto después de procesar el modelo.
	load_effect("text.effect");
	// Libera la escena de Assimp para evitar fugas de memoria.
	aiReleaseImport(scene);
	blog(LOG_INFO, "Modelo cargado con %zu mallas", g_mesh_count);
	return true;
}

/**
 * @brief Renderiza todas las mallas del modelo cargado.
 *
 * Esta función debe ser llamada en cada fotograma dentro del ciclo de renderizado de OBS.
 */
void render_model_c()
{
	/* 
	if (!effect)
		return;

	// Obtiene el parámetro 'image' del efecto para poder asignarle la textura.
	gs_eparam_t *image_param = gs_effect_get_param_by_name(effect, "image");

	// Obtiene la técnica de renderizado "Draw" del efecto.
	gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
	if (!tech)
		return;

	// Inicia el proceso de renderizado con la técnica y el primer paso (pass).
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	*/

	// Itera sobre cada malla del modelo.
	for (size_t i = 0; i < g_mesh_count; i++) {
		// Se salta la malla si sus buffers no son válidos.
		if (!g_meshes[i].vb || !g_meshes[i].ib)
			continue;

		/* 
		   Si no hay textura, pasaría NULL.
		if (g_meshes[i].texture)
			gs_effect_set_texture(image_param, g_meshes[i].texture);
		else
			gs_effect_set_texture(image_param, NULL);
		*/

		
		gs_load_vertexbuffer(g_meshes[i].vb);
		gs_load_indexbuffer(g_meshes[i].ib);

		// Lanza la llamada de dibujado (draw call) para renderizar los triángulos.
		// GS_TRIS indica que se dibujarán triángulos.
		// '0' es el índice de inicio.
		// `g_meshes[i].num_indices` es el número de índices a procesar.
		gs_draw(GS_TRIS, 0, g_meshes[i].num_indices);
	}

	/* Este bloque finaliza el paso y la técnica del efecto.
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
	*/
}