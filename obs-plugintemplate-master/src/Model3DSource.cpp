// Model3DSource.cpp

#include "Model3DSource.hpp"
#include <QFile>
#include <QTextStream>
#include <QDebug>

/**
 * @brief Constructs a Model3DSource object.
 *
 * Initializes OpenGL functions, loads a 3D model from the specified path,
 * and sets up the necessary OpenGL objects for rendering, including a Vertex
 * Array Object (VAO), a Vertex Buffer Object (VBO), and the shader program.
 *
 * @param path The file path to the 3D model (e.g., an .obj file).
 */
Model3DSource::Model3DSource(const QString &path)
	: vbo(QOpenGLBuffer::VertexBuffer)
{
	initializeOpenGLFunctions();
	loadModel(path);
	vao.create();
	vao.bind();

	vbo.create();
	vbo.bind();
	vbo.allocate(vertices.data(), vertices.size() * sizeof(float));

	program.addShaderFromSourceFile(QOpenGLShader::Vertex,
					":/shaders/simple.vert");
	program.addShaderFromSourceFile(QOpenGLShader::Fragment,
					":/shaders/simple.frag");
	program.link();
	program.bind();

	program.enableAttributeArray(0);
	program.setAttributeBuffer(0, GL_FLOAT, 0, 3);

	vao.release();
	vbo.release();
	program.release();
}

/**
 * @brief Destroys the Model3DSource object.
 *
 * Cleans up and releases the OpenGL resources, specifically the VAO and VBO,
 * to prevent memory leaks.
 */
Model3DSource::~Model3DSource()
{
	vao.destroy();
	vbo.destroy();
}

/**
 * @brief Loads a 3D model from a file.
 *
 * Reads vertex data from a file (presumably in .obj format). It parses lines
 * starting with "v " to extract vertex coordinates and populates the `vertices`
 * vector.
 *
 * @param path The file path to the 3D model.
 */
void Model3DSource::loadModel(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "No se pudo abrir el modelo:" << path;
		return;
	}

	QTextStream in(&file);
	while (!in.atEnd()) {
		QString line = in.readLine();
		if (line.startsWith("v ")) {
			QStringList parts = line.split(" ", Qt::SkipEmptyParts);
			if (parts.size() == 4) {
				vertices.push_back(parts[1].toFloat());
				vertices.push_back(parts[2].toFloat());
				vertices.push_back(parts[3].toFloat());
			}
		}
	}
}

/**
 * @brief Renders the 3D model.
 *
 * Sets up the projection matrix based on the viewport dimensions, binds the
 * shader program and VAO, sets the model-view-projection (MVP) uniform,
 * and issues the draw call to render the model.
 *
 * @param width The width of the viewport.
 * @param height The height of the viewport.
 */
void Model3DSource::render(int width, int height)
{
	projectionMatrix.setToIdentity();
	projectionMatrix.perspective(45.0f, (float)width / (float)height, 0.1f,
				     100.0f);

	program.bind();
	vao.bind();

	program.setUniformValue("mvp", projectionMatrix);
	glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 3);

	vao.release();
	program.release();
}
