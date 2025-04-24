// Model3DSource.cpp

#include "Model3DSource.hpp"
#include <QFile>
#include <QTextStream>
#include <QDebug>

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

Model3DSource::~Model3DSource()
{
	vao.destroy();
	vbo.destroy();
}

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
