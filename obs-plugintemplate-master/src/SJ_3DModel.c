	// src/ModelLoader.c
	#include <assimp/cimport.h>
	#include <assimp/scene.h>
	#include <assimp/postprocess.h>
	#include <obs-module.h>
	#include <graphics/graphics.h> // Para gs_vertexbuffer_*, gs_indexbuffer_*
	#include <graphics/image-file.h>
	#include "graphics/quat.h"
	#include <util/platform.h> // bmemdup, bmalloc, etc. (memoria multiplataforma)
	#include "obs-config.h"
	#include <obs-module.h>

	// Estructura para una malla cargada
	typedef struct {
		gs_vertbuffer_t *vb;
		gs_indexbuffer_t *ib;
		uint32_t num_indices;
		uint32_t num_vertex;
	} Mesh;

	// Array dinámico de mallas; aquí lo simplificamos como un puntero y contador //esta d¡feo esto no? no deberia ser estatico?
	static Mesh *g_meshes = NULL;
	static size_t g_mesh_count = 0;

	bool load_model_c(const char *path)
	{
		// Importa la escena (triangula, flip UVs, calcula tangentes/normales)
		const struct aiScene *scene = aiImportFile(path, aiProcess_Triangulate | aiProcess_FlipUVs |
						   aiProcess_CalcTangentSpace);

		if (!scene) {
			blog(LOG_ERROR, "Assimp C error: %s", aiGetErrorString());
			return false;
		}

		// Liberar memoria previa
		if (g_meshes) {
			for (size_t i = 0; i < g_mesh_count; i++) {
				gs_vertexbuffer_destroy(g_meshes[i].vb);
				gs_indexbuffer_destroy(g_meshes[i].ib);
			}
			bfree(g_meshes);
			g_meshes = NULL;
			g_mesh_count = 0;
		}

		// Reserva espacio para todas las mallas
		g_mesh_count = scene->mNumMeshes;
		g_meshes = bmalloc(sizeof(Mesh) * g_mesh_count);

		// Recorre cada aiMesh
		for (size_t m = 0; m < g_mesh_count; m++) {
	

			// 1) Empaquetar vértices: posición (3 floats) + normales (3 floats)
			struct aiMesh *mesh = scene->mMeshes[m];
			size_t vert_count = mesh->mNumVertices;
			size_t idx_count = mesh->mNumFaces * 3;

			// 1) Posiciones
			float *positions = bmalloc(sizeof(float) * 3 * vert_count);
			for (size_t i = 0; i < vert_count; i++) {
				positions[i * 3 + 0] = mesh->mVertices[i].x;
				positions[i * 3 + 1] = mesh->mVertices[i].y;
				positions[i * 3 + 2] = mesh->mVertices[i].z;
			}

			// 2) Normales
			float *normals = bmalloc(sizeof(float) * 3 * vert_count);
			for (size_t i = 0; i < vert_count; i++) {
				normals[i * 3 + 0] = mesh->mNormals[i].x;
				normals[i * 3 + 1] = mesh->mNormals[i].y;
				normals[i * 3 + 2] = mesh->mNormals[i].z;
			}

			// 2) Empaquetar índices (ya triangulados)
		
			uint32_t *indices = bmalloc(sizeof(uint32_t) * idx_count);
			for (size_t f = 0; f < mesh->mNumFaces; f++) {
				const struct aiFace *face = &mesh->mFaces[f];
				indices[f * 3 + 0] = face->mIndices[0];
				indices[f * 3 + 1] = face->mIndices[1];
				indices[f * 3 + 2] = face->mIndices[2];
			}
	
			obs_enter_graphics();
			gs_render_start(true);

			struct gs_vb_data *vbd = gs_vbdata_create();
			vbd->num = (uint32_t)vert_count;
			vbd->points = bmemdup(positions, sizeof(float) * 3 * vert_count);
			vbd->normals = bmemdup(normals, sizeof(float) * 3 * vert_count);
			// 3) Crear buffers de libOBS (wrapper OpenGL/D3D11)
			gs_vertbuffer_t *vb = gs_vertexbuffer_create(
				vbd,
				0
			);
			gs_indexbuffer_t *ib = gs_indexbuffer_create(
				GS_UNSIGNED_SHORT, indices, idx_count, 0);
	
			// Guardar en nuestro array
			g_meshes[m].vb = vb;
			g_meshes[m].ib = ib;
			g_meshes[m].num_indices = (uint32_t)idx_count;
			g_meshes[m].num_vertex = vert_count;
			obs_leave_graphics();
			// Liberar arrays temporales
			bfree(positions);
			bfree(indices);
		}

		aiReleaseImport(scene);
		blog(LOG_INFO, "Modelo C cargado: %zu mallas", g_mesh_count);
		return true;
	}

	// Llamar desde tu callback de render (video_render)
	void render_model_c(void)
	{
		gs_load_vertexbuffer(g_meshes->vb);
		gs_load_indexbuffer(g_meshes->ib);
		gs_draw(GS_TRIS, 0, g_meshes->num_vertex);
	
	}
